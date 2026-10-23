#include "media-countdown.hpp"
#include "plugin-settings.hpp"
#include "plugin-support.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QBoxLayout>
#include <QDateTime>
#include <QEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

constexpr int kTickMs = 100;
constexpr int kBlinkMs = 400;

const QColor kWarnColor(0xf5, 0xa6, 0x23);
const QColor kCriticalColor(0xe5, 0x48, 0x4d);
const QColor kCriticalDimColor(0x8b, 0x1e, 0x24);

struct MediaInfo {
	QString name;
	obs_media_state state = OBS_MEDIA_STATE_NONE;
	qint64 durationMs = 0;
	qint64 elapsedMs = 0;
	qint64 remainingMs = 0; /* wall-clock time, playback speed applied */
	bool looping = false;
};

void CollectMediaSource(obs_source_t *, obs_source_t *child, void *param)
{
	auto *sources = static_cast<std::vector<OBSSource> *>(param);
	if (!(obs_source_get_output_flags(child) & OBS_SOURCE_CONTROLLABLE_MEDIA))
		return;

	for (const OBSSource &source : *sources) {
		if (source.Get() == child)
			return;
	}
	sources->emplace_back(child);
}

/* The source that is really on the Program output right now */
OBSSourceAutoRelease ProgramRoot()
{
	OBSSourceAutoRelease output = obs_get_output_source(0);
	if (output && obs_source_get_type(output) == OBS_SOURCE_TYPE_TRANSITION)
		return obs_transition_get_active_source(output);
	if (output)
		return output;
	return obs_frontend_get_current_scene();
}

std::vector<MediaInfo> ProgramMedia()
{
	std::vector<MediaInfo> result;

	OBSSourceAutoRelease root = ProgramRoot();
	if (!root)
		return result;

	std::vector<OBSSource> sources;
	CollectMediaSource(nullptr, root, &sources);
	obs_source_enum_active_tree(root, CollectMediaSource, &sources);

	for (const OBSSource &source : sources) {
		MediaInfo info;
		info.state = obs_source_media_get_state(source);
		if (info.state != OBS_MEDIA_STATE_PLAYING && info.state != OBS_MEDIA_STATE_PAUSED &&
		    info.state != OBS_MEDIA_STATE_BUFFERING && info.state != OBS_MEDIA_STATE_OPENING)
			continue;

		info.name = QString::fromUtf8(obs_source_get_name(source));
		info.durationMs = std::max<qint64>(0, obs_source_media_get_duration(source));
		info.elapsedMs = std::max<qint64>(0, obs_source_media_get_time(source));
		if (info.durationMs > 0)
			info.elapsedMs = std::min(info.elapsedMs, info.durationMs);

		double speed = 1.0;
		const char *id = obs_source_get_unversioned_id(source);
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		if (id && strcmp(id, "ffmpeg_source") == 0) {
			info.looping = obs_data_get_bool(settings, "looping");
			const long long percent = obs_data_get_int(settings, "speed_percent");
			if (percent > 0)
				speed = percent / 100.0;
		} else if (id && strcmp(id, "vlc_source") == 0) {
			info.looping = obs_data_get_bool(settings, "loop");
		}

		if (info.durationMs > 0)
			info.remainingMs = (qint64)((info.durationMs - info.elapsedMs) / speed);

		result.push_back(info);
	}

	/* The countdown follows the "main" media: one with a known length,
	 * not looping, playing rather than paused, and ending last. */
	std::stable_sort(result.begin(), result.end(), [](const MediaInfo &a, const MediaInfo &b) {
		const bool aTimed = a.durationMs > 0, bTimed = b.durationMs > 0;
		if (aTimed != bTimed)
			return aTimed;
		if (a.looping != b.looping)
			return !a.looping;
		const bool aPaused = a.state == OBS_MEDIA_STATE_PAUSED;
		const bool bPaused = b.state == OBS_MEDIA_STATE_PAUSED;
		if (aPaused != bPaused)
			return !aPaused;
		return a.remainingMs > b.remainingMs;
	});

	return result;
}

QString Clock(qint64 totalSeconds)
{
	const qint64 hours = totalSeconds / 3600;
	const qint64 minutes = (totalSeconds / 60) % 60;
	const qint64 seconds = totalSeconds % 60;
	if (hours > 0)
		return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	return QStringLiteral("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

/* Rounded up, so "00:00" only appears once the media has really ended */
QString FormatRemaining(qint64 ms, bool tenths)
{
	ms = std::max<qint64>(0, ms);
	if (tenths) {
		const qint64 deciseconds = (ms + 99) / 100;
		return QStringLiteral("-%1.%2").arg(Clock(deciseconds / 10)).arg(deciseconds % 10);
	}
	return QStringLiteral("-%1").arg(Clock((ms + 999) / 1000));
}

void DrawStateIcon(QPainter &painter, const QRectF &rect, bool paused, const QColor &color)
{
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);

	if (paused) {
		const qreal bar = rect.width() * 0.32;
		painter.drawRoundedRect(QRectF(rect.left() + rect.width() * 0.08, rect.top(), bar, rect.height()), 1, 1);
		painter.drawRoundedRect(QRectF(rect.right() - rect.width() * 0.08 - bar, rect.top(), bar, rect.height()),
					1, 1);
		return;
	}

	QPainterPath triangle;
	triangle.moveTo(rect.left() + rect.width() * 0.12, rect.top());
	triangle.lineTo(rect.right(), rect.center().y());
	triangle.lineTo(rect.left() + rect.width() * 0.12, rect.bottom());
	triangle.closeSubpath();
	painter.drawPath(triangle);
}

QColor WithAlpha(QColor color, qreal alpha)
{
	color.setAlphaF(alpha);
	return color;
}

} // namespace

/* ------------------------------------------------------------------------ */

CountdownView::CountdownView(QWidget *parent) : QWidget(parent)
{
	setObjectName(QStringLiteral("sceneColorCountdownBar"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setAccessibleName(QTStr("Countdown.AccessibleName"));
}

QFont CountdownView::TimeFont() const
{
	QFont timeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	timeFont.setPointSizeF(std::max<qreal>(11.0, QFontInfo(font()).pointSizeF() * 1.3));
	timeFont.setBold(true);
	return timeFont;
}

QSize CountdownView::sizeHint() const
{
	const int height = std::max(fontMetrics().height(), QFontMetrics(TimeFont()).height()) + 10;
	return {320, height};
}

QSize CountdownView::minimumSizeHint() const
{
	return {120, sizeHint().height()};
}

void CountdownView::SetState(const CountdownState &newState)
{
	if (newState == state)
		return;

	if (newState.tooltip != state.tooltip)
		setToolTip(newState.tooltip);

	state = newState;
	update();
}

void CountdownView::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
		updateGeometry();
	QWidget::changeEvent(event);
}

void CountdownView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QPalette pal = palette();
	const QColor text = pal.color(QPalette::WindowText);
	const QColor dim = WithAlpha(text, 0.55);
	const QColor accent = pal.color(QPalette::Highlight);
	const QRect area = rect().adjusted(8, 4, -8, -4);

	/* separator line toward the display above */
	painter.fillRect(QRect(0, 0, width(), 1), WithAlpha(text, 0.12));

	if (state.level == 2)
		painter.fillRect(rect().adjusted(0, 1, 0, 0), WithAlpha(kCriticalColor, state.blinkOn ? 0.10 : 0.22));

	/* remaining time, right aligned, fixed width so nothing jumps around */
	const QFont timeFont = TimeFont();
	const QFontMetrics timeMetrics(timeFont);
	QString remaining = QStringLiteral("--:--");
	if (state.hasMedia)
		remaining = state.hasDuration ? FormatRemaining(state.remainingMs, state.level > 0)
					      : QTStr("Countdown.Live");

	const QString widthTemplate = state.durationMs >= 3600 * 1000 ? QStringLiteral("-00:00:00.0")
								       : QStringLiteral("-00:00.0");
	const int pillWidth =
		std::max(timeMetrics.horizontalAdvance(widthTemplate), timeMetrics.horizontalAdvance(remaining)) + 16;
	const QRect pill(area.right() - pillWidth + 1, area.top(), pillWidth, area.height());

	QColor pillText = state.hasMedia ? text : dim;
	if (state.level == 1) {
		painter.setPen(Qt::NoPen);
		painter.setBrush(kWarnColor);
		painter.drawRoundedRect(pill, 4, 4);
		pillText = QColor(24, 24, 24);
	} else if (state.level == 2) {
		painter.setPen(Qt::NoPen);
		painter.setBrush(state.blinkOn ? kCriticalDimColor : kCriticalColor);
		painter.drawRoundedRect(pill, 4, 4);
		pillText = Qt::white;
	}
	painter.setFont(timeFont);
	painter.setPen(pillText);
	painter.drawText(pill, Qt::AlignCenter, remaining);

	painter.setFont(font());
	const QFontMetrics metrics = fontMetrics();
	int right = pill.left() - 10;

	if (!state.hasMedia) {
		const QRect idleRect(area.left(), area.top(), std::max(0, right - area.left()), area.height());
		painter.setPen(dim);
		painter.drawText(idleRect, Qt::AlignVCenter | Qt::AlignLeft,
				 metrics.elidedText(QTStr("Countdown.Idle"), Qt::ElideRight, idleRect.width()));
		return;
	}

	/* "+N" when more media is playing in Program (details in the tooltip) */
	if (state.others > 0) {
		const QString badge = QStringLiteral("+%1").arg(state.others);
		const int badgeWidth = metrics.horizontalAdvance(badge) + 12;
		const QRect badgeRect(right - badgeWidth, area.center().y() - metrics.height() / 2, badgeWidth,
				      metrics.height());
		painter.setPen(Qt::NoPen);
		painter.setBrush(WithAlpha(text, 0.16));
		painter.drawRoundedRect(badgeRect, badgeRect.height() / 2.0, badgeRect.height() / 2.0);
		painter.setPen(text);
		painter.drawText(badgeRect, Qt::AlignCenter, badge);
		right = badgeRect.left() - 8;
	}

	if (state.showElapsed && state.hasDuration) {
		const QString times = Clock(state.elapsedMs / 1000) + QStringLiteral(" / ") +
				      Clock((state.durationMs + 500) / 1000);
		const int timesWidth = metrics.horizontalAdvance(times);
		if (right - area.left() > timesWidth + 160) {
			painter.setPen(dim);
			painter.drawText(QRect(right - timesWidth, area.top(), timesWidth, area.height()),
					 Qt::AlignVCenter | Qt::AlignRight, times);
			right -= timesWidth + 12;
		}
	}

	const int iconSize = std::min(area.height(), 11);
	const QRectF iconRect(area.left(), area.center().y() - iconSize / 2.0 + 0.5, iconSize, iconSize);
	QColor iconColor = state.level == 2 ? kCriticalColor : state.level == 1 ? kWarnColor : accent;
	DrawStateIcon(painter, iconRect, state.paused, state.paused ? dim : iconColor);
	int left = area.left() + iconSize + 8;

	QString label = state.name;
	if (state.looping)
		label += QStringLiteral(" ↻");
	if (state.paused)
		label += QStringLiteral(" · ") + QTStr("Countdown.Paused");

	const int available = std::max(0, right - left);
	const int nameWidthMax = state.hasDuration ? std::max(80, available * 2 / 5) : available;
	const QString elided = metrics.elidedText(label, Qt::ElideRight, std::min(nameWidthMax, available));
	const int nameWidth = metrics.horizontalAdvance(elided);
	painter.setPen(state.paused ? dim : text);
	painter.drawText(QRect(left, area.top(), nameWidth, area.height()), Qt::AlignVCenter | Qt::AlignLeft, elided);
	left += nameWidth + 12;

	if (state.hasDuration && right - left > 30) {
		const QRectF track(left, area.center().y() - 2.5, right - left, 5);
		painter.setPen(Qt::NoPen);
		painter.setBrush(WithAlpha(text, 0.15));
		painter.drawRoundedRect(track, 2.5, 2.5);

		const double progress =
			std::clamp(double(state.elapsedMs) / double(std::max<qint64>(1, state.durationMs)), 0.0, 1.0);
		QRectF fill = track;
		fill.setWidth(track.width() * progress);
		if (fill.width() >= 1.0) {
			painter.setBrush(state.paused ? dim : iconColor);
			painter.drawRoundedRect(fill, 2.5, 2.5);
		}
	}
}

/* ------------------------------------------------------------------------ */

MediaCountdownController::MediaCountdownController(QMainWindow *main_) : QObject(main_), main(main_)
{
	timer.setInterval(kTickMs);
	connect(&timer, &QTimer::timeout, this, &MediaCountdownController::Tick);
	UpdatePlacement();
}

MediaCountdownController::~MediaCountdownController()
{
	timer.stop();
	delete regularView.data();
	delete studioView.data();
}

QWidget *MediaCountdownController::FindProgramContainer() const
{
	if (!main)
		return nullptr;

	/* Studio mode adds a widget holding [label, program display] to the
	 * same layout as the preview (OBS 32: OBSBasic::SetPreviewProgramMode) */
	auto *previewLayout = main->findChild<QBoxLayout *>(QStringLiteral("previewLayout"));
	if (!previewLayout)
		return nullptr;

	for (int i = previewLayout->count() - 1; i >= 0; i--) {
		QWidget *widget = previewLayout->itemAt(i)->widget();
		if (!widget || widget->objectName() == QLatin1String("previewContainer") ||
		    !qobject_cast<QBoxLayout *>(widget->layout()))
			continue;

		const auto children = widget->findChildren<QWidget *>(Qt::FindDirectChildrenOnly);
		for (QWidget *child : children) {
			if (child->inherits("OBSQTDisplay") && child->objectName() != QLatin1String("preview"))
				return widget;
		}
	}
	return nullptr;
}

void MediaCountdownController::UpdatePlacement()
{
	if (!main)
		return;

	const bool enabled = Settings().countdownEnabled;
	const bool studio = obs_frontend_preview_program_mode_active();

	if (!regularView) {
		/* Right under the canvas area and above the source context bar,
		 * outside the preview container so it survives "Disable Preview" */
		QWidget *canvas = main->findChild<QWidget *>(QStringLiteral("canvasEditor"));
		QWidget *parent = canvas ? canvas->parentWidget() : nullptr;
		auto *layout = parent ? qobject_cast<QBoxLayout *>(parent->layout()) : nullptr;
		const int index = layout ? layout->indexOf(canvas) : -1;
		if (index >= 0) {
			regularView = new CountdownView(parent);
			layout->insertWidget(index + 1, regularView);
		} else {
			plog(LOG_WARNING, "Canvas area not found, countdown unavailable in regular mode");
		}
	}
	if (regularView)
		regularView->setVisible(enabled && !studio);

	/* The program widget is rebuilt every time studio mode is enabled, and
	 * our bar is destroyed with it when studio mode is disabled. */
	if (studio && enabled && !studioView) {
		QWidget *container = FindProgramContainer();
		auto *layout = container ? qobject_cast<QBoxLayout *>(container->layout()) : nullptr;
		if (layout) {
			studioView = new CountdownView(container);
			layout->addWidget(studioView);
		} else {
			plog(LOG_WARNING, "Program display not found, countdown unavailable in studio mode");
		}
	}
	if (studioView)
		studioView->setVisible(enabled && studio);

	const bool shown = (regularView && !regularView->isHidden()) || (studioView && !studioView->isHidden());
	if (shown) {
		if (!timer.isActive())
			timer.start();
		Tick();
	} else {
		timer.stop();
	}
}

void MediaCountdownController::Tick()
{
	const PluginSettings &cfg = Settings();
	const std::vector<MediaInfo> media = ProgramMedia();

	CountdownState state;
	state.showElapsed = cfg.showElapsed;

	if (!media.empty()) {
		const MediaInfo &primary = media.front();
		state.hasMedia = true;
		state.name = primary.name;
		state.paused = primary.state == OBS_MEDIA_STATE_PAUSED;
		state.looping = primary.looping;
		state.hasDuration = primary.durationMs > 0;
		state.elapsedMs = primary.elapsedMs;
		state.durationMs = primary.durationMs;
		state.remainingMs = primary.remainingMs;
		state.others = (int)media.size() - 1;

		if (state.hasDuration) {
			if (primary.remainingMs <= cfg.criticalSeconds * 1000LL)
				state.level = 2;
			else if (primary.remainingMs <= cfg.warnSeconds * 1000LL)
				state.level = 1;
		}

		state.blinkOn = state.level == 2 && cfg.blinkCritical && !state.paused &&
				(QDateTime::currentMSecsSinceEpoch() / kBlinkMs) % 2 == 1;

		if (state.others > 0) {
			QStringList lines;
			lines << QTStr("Countdown.AllMedia");
			for (const MediaInfo &info : media) {
				const QString time = info.durationMs > 0 ? FormatRemaining(info.remainingMs, false)
									 : QTStr("Countdown.Live");
				lines << QStringLiteral("• %1  %2").arg(info.name, time);
			}
			state.tooltip = lines.join(QLatin1Char('\n'));
		}
	}

	if (regularView && !regularView->isHidden())
		regularView->SetState(state);
	if (studioView && !studioView->isHidden())
		studioView->SetState(state);
}
