#include "media-countdown.hpp"
#include "plugin-settings.hpp"
#include "plugin-support.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>

#include <QBoxLayout>
#include <QDateTime>
#include <QEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int kTickMs = 100;
constexpr int kBlinkMs = 400;
constexpr int kSeekThrottleMs = 80;

/* ffmpeg sources report PLAYING after leaving Program although playback was
 * stopped, so a "playing" preview media whose time does not move for this
 * long is treated as stopped (Play then restarts it) */
constexpr qint64 kStallMs = 1000;

const QColor kWarnColor(0xf5, 0xa6, 0x23);
const QColor kCriticalColor(0xe5, 0x48, 0x4d);
const QColor kCriticalDimColor(0x8b, 0x1e, 0x24);

struct MediaInfo {
	OBSSource source;
	QString name;
	obs_media_state state = OBS_MEDIA_STATE_NONE;
	qint64 durationMs = 0;
	qint64 elapsedMs = 0;
	qint64 remainingMs = 0; /* wall-clock time, playback speed applied */
	bool looping = false;
	bool onAir = false;
};

bool IsBusy(obs_media_state state)
{
	return state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_PAUSED ||
	       state == OBS_MEDIA_STATE_BUFFERING || state == OBS_MEDIA_STATE_OPENING;
}

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

/* Media under root (nested scenes and groups included). Only playing or
 * paused media unless includeIdle. */
std::vector<MediaInfo> CollectMedia(obs_source_t *root, bool includeIdle)
{
	std::vector<MediaInfo> result;
	if (!root)
		return result;

	std::vector<OBSSource> sources;
	CollectMediaSource(nullptr, root, &sources);
	obs_source_enum_active_tree(root, CollectMediaSource, &sources);

	for (const OBSSource &source : sources) {
		MediaInfo info;
		info.state = obs_source_media_get_state(source);
		if (!includeIdle && !IsBusy(info.state))
			continue;

		info.source = source;
		info.name = QString::fromUtf8(obs_source_get_name(source));
		info.onAir = obs_source_active(source);
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
				speed = double(percent) / 100.0;
		} else if (id && strcmp(id, "vlc_source") == 0) {
			info.looping = obs_data_get_bool(settings, "loop");
		}

		if (info.durationMs > 0)
			info.remainingMs = (qint64)(double(info.durationMs - info.elapsedMs) / speed);

		result.push_back(info);
	}

	return result;
}

/* Program follows the "main" media: known length, not looping, playing
 * rather than paused, and ending last. */
void SortForProgram(std::vector<MediaInfo> &media)
{
	std::stable_sort(media.begin(), media.end(), [](const MediaInfo &a, const MediaInfo &b) {
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
}

/* Preview prefers media it may control: not live in Program, not looping,
 * already started, then the longest. Not-started media has no length yet. */
void SortForPreview(std::vector<MediaInfo> &media)
{
	std::stable_sort(media.begin(), media.end(), [](const MediaInfo &a, const MediaInfo &b) {
		if (a.onAir != b.onAir)
			return !a.onAir;
		if (a.looping != b.looping)
			return !a.looping;
		const bool aBusy = IsBusy(a.state), bBusy = IsBusy(b.state);
		if (aBusy != bBusy)
			return aBusy;
		return a.remainingMs > b.remainingMs;
	});
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

QString MediaListTooltip(const char *headerKey, const std::vector<MediaInfo> &media, bool markOnAir)
{
	QStringList lines;
	lines << QTStr(headerKey);
	for (const MediaInfo &info : media) {
		QString time;
		if (info.durationMs > 0)
			time = FormatRemaining(info.remainingMs, false);
		else
			time = IsBusy(info.state) ? QTStr("Countdown.Live") : QStringLiteral("--:--");
		if (markOnAir && info.onAir)
			time += QStringLiteral("  ") + QTStr("Tally.Program");
		lines << QStringLiteral("• %1  %2").arg(info.name, time);
	}
	return lines.join(QLatin1Char('\n'));
}

void DrawStateIcon(QPainter &painter, const QRectF &rect, bool paused, bool stopped, const QColor &color)
{
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);

	if (stopped) {
		painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 1.5, 1.5);
		return;
	}

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
	color.setAlphaF(float(alpha));
	return color;
}

} // namespace

/* ------------------------------------------------------------------------ */

CountdownView::CountdownView(QWidget *parent) : QWidget(parent)
{
	setObjectName(QStringLiteral("sceneColorCountdownBar"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMouseTracking(true);
	setAccessibleName(QTStr("Countdown.AccessibleName"));
}

void CountdownView::SetDrawSeparator(bool draw)
{
	drawSeparator = draw;
	update();
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
	if (!state.seekable && dragging)
		dragging = false;
	update();
}

void CountdownView::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
		updateGeometry();
	QWidget::changeEvent(event);
}

bool CountdownView::OverTrack(const QPointF &pos) const
{
	return state.seekable && !trackRect.isEmpty() && trackRect.adjusted(-4, -7, 4, 7).contains(pos);
}

double CountdownView::FractionAt(const QPointF &pos) const
{
	if (trackRect.width() <= 0)
		return 0.0;
	return std::clamp((pos.x() - trackRect.left()) / trackRect.width(), 0.0, 1.0);
}

void CountdownView::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && seekRequested && OverTrack(event->position())) {
		dragging = true;
		dragFraction = FractionAt(event->position());
		seekRequested(dragFraction, false);
		update();
		return;
	}
	QWidget::mousePressEvent(event);
}

void CountdownView::mouseMoveEvent(QMouseEvent *event)
{
	if (dragging) {
		dragFraction = FractionAt(event->position());
		if (seekRequested)
			seekRequested(dragFraction, false);
		update();
		return;
	}

	if (OverTrack(event->position()))
		setCursor(Qt::PointingHandCursor);
	else
		unsetCursor();
	QWidget::mouseMoveEvent(event);
}

void CountdownView::mouseReleaseEvent(QMouseEvent *event)
{
	if (dragging && event->button() == Qt::LeftButton) {
		dragging = false;
		dragFraction = FractionAt(event->position());
		if (seekRequested)
			seekRequested(dragFraction, true);
		update();
		return;
	}
	QWidget::mouseReleaseEvent(event);
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

	/* while dragging on the progress bar, show the position being picked */
	qint64 elapsed = state.elapsedMs;
	qint64 remaining = state.remainingMs;
	if (dragging && state.durationMs > 0) {
		elapsed = (qint64)(dragFraction * double(state.durationMs));
		remaining = state.durationMs - elapsed;
	}

	if (drawSeparator)
		painter.fillRect(QRect(0, 0, width(), 1), WithAlpha(text, 0.12));

	if (state.level == 2)
		painter.fillRect(rect().adjusted(0, 1, 0, 0), WithAlpha(kCriticalColor, state.blinkOn ? 0.10 : 0.22));

	/* remaining time, right aligned, fixed width so nothing jumps around */
	const QFont timeFont = TimeFont();
	const QFontMetrics timeMetrics(timeFont);
	QString remainingText = QStringLiteral("--:--");
	if (state.hasMedia && state.hasDuration)
		remainingText = FormatRemaining(remaining, state.level > 0);
	else if (state.hasMedia && !state.stopped)
		remainingText = QTStr("Countdown.Live");

	const QString widthTemplate = state.durationMs >= 3600 * 1000 ? QStringLiteral("-00:00:00.0")
								       : QStringLiteral("-00:00.0");
	const int pillWidth = std::max(timeMetrics.horizontalAdvance(widthTemplate),
				       timeMetrics.horizontalAdvance(remainingText)) +
			      16;
	const QRect pill(area.right() - pillWidth + 1, area.top(), pillWidth, area.height());

	QColor pillText = state.hasMedia && !state.stopped ? text : dim;
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
	painter.drawText(pill, Qt::AlignCenter, remainingText);

	painter.setFont(font());
	const QFontMetrics metrics = fontMetrics();
	int right = pill.left() - 10;
	trackRect = QRectF();

	if (!state.hasMedia) {
		const QString idle = state.idleText.isEmpty() ? QTStr("Countdown.Idle") : state.idleText;
		const QRect idleRect(area.left(), area.top(), std::max(0, right - area.left()), area.height());
		painter.setPen(dim);
		painter.drawText(idleRect, Qt::AlignVCenter | Qt::AlignLeft,
				 metrics.elidedText(idle, Qt::ElideRight, idleRect.width()));
		return;
	}

	/* "+N" when more media is in the scene (details in the tooltip) */
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
		const QString times = Clock(elapsed / 1000) + QStringLiteral(" / ") + Clock((state.durationMs + 500) / 1000);
		const int timesWidth = metrics.horizontalAdvance(times);
		if (right - area.left() > timesWidth + 160) {
			painter.setPen(dim);
			painter.drawText(QRect(right - timesWidth, area.top(), timesWidth, area.height()),
					 Qt::AlignVCenter | Qt::AlignRight, times);
			right -= timesWidth + 12;
		}
	}

	const bool inactive = state.paused || state.stopped;
	const int iconSize = std::min(area.height(), 11);
	const QRectF iconRect(area.left(), area.center().y() - iconSize / 2.0 + 0.5, iconSize, iconSize);
	const QColor stateColor = state.level == 2 ? kCriticalColor : state.level == 1 ? kWarnColor : accent;
	DrawStateIcon(painter, iconRect, state.paused, state.stopped, inactive ? dim : stateColor);
	int left = area.left() + iconSize + 8;

	QString label = state.name;
	if (state.looping)
		label += QStringLiteral(" ↻");
	if (!state.status.isEmpty())
		label += QStringLiteral(" · ") + state.status;

	/* blinking ON AIR dot right after the name when this media is live in Program */
	constexpr qreal kDotRadius = 4.0;
	const int onAirWidth = state.onAir ? (int)std::ceil(2 * (kDotRadius + 2.5)) : 0;

	const int available = std::max(0, right - left - (onAirWidth ? onAirWidth + 8 : 0));
	const int nameWidthMax = state.hasDuration ? std::max(80, available * 2 / 5) : available;
	const QString elided = metrics.elidedText(label, Qt::ElideRight, std::min(nameWidthMax, available));
	const int nameWidth = metrics.horizontalAdvance(elided);
	painter.setPen(inactive ? dim : text);
	painter.drawText(QRect(left, area.top(), nameWidth, area.height()), Qt::AlignVCenter | Qt::AlignLeft, elided);
	left += nameWidth + 8;

	if (onAirWidth) {
		DrawOnAirDot(painter, QPointF(left + onAirWidth / 2.0, area.center().y() + 0.5), kDotRadius,
			     Settings().tallyProgramColor, state.onAirBlinkOn);
		left += onAirWidth + 8;
	}
	left += 4;

	if (state.hasDuration && right - left > 30) {
		const QRectF track(left, area.center().y() - 2.5, right - left, 5);
		trackRect = track;
		painter.setPen(Qt::NoPen);
		painter.setBrush(WithAlpha(text, 0.15));
		painter.drawRoundedRect(track, 2.5, 2.5);

		const double progress =
			std::clamp(double(elapsed) / double(std::max<qint64>(1, state.durationMs)), 0.0, 1.0);
		QRectF fill = track;
		fill.setWidth(track.width() * progress);
		const QColor fillColor = inactive && !dragging ? dim : stateColor;
		if (fill.width() >= 1.0) {
			painter.setBrush(fillColor);
			painter.drawRoundedRect(fill, 2.5, 2.5);
		}

		/* a handle shows that the bar can be dragged */
		if (state.seekable) {
			painter.setBrush(fillColor);
			painter.drawEllipse(QPointF(fill.right(), track.center().y()), 5, 5);
		}
	}
}

/* ------------------------------------------------------------------------ */

PreviewMediaBar::PreviewMediaBar(QWidget *parent) : QWidget(parent)
{
	setObjectName(QStringLiteral("sceneColorCountdownPreviewBar"));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	auto makeButton = [this](const char *textKey) {
		auto *button = new QToolButton(this);
		button->setAutoRaise(true);
		button->setToolTip(QTStr(textKey));
		button->setAccessibleName(QTStr(textKey));
		button->setIconSize(QSize(14, 14));
		return button;
	};
	playPauseButton = makeButton("Preview.Play");
	restartButton = makeButton("Preview.Restart");
	stopButton = makeButton("Preview.Stop");

	view = new CountdownView(this);
	view->SetDrawSeparator(false);
	view->setAccessibleName(QTStr("Preview.AccessibleName"));

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(6, 1, 0, 0);
	layout->setSpacing(2);
	layout->addWidget(playPauseButton);
	layout->addWidget(restartButton);
	layout->addWidget(stopButton);
	layout->addWidget(view, 1);

	connect(playPauseButton, &QToolButton::clicked, this, [this]() {
		if (playPauseClicked)
			playPauseClicked();
	});
	connect(restartButton, &QToolButton::clicked, this, [this]() {
		if (restartClicked)
			restartClicked();
	});
	connect(stopButton, &QToolButton::clicked, this, [this]() {
		if (stopClicked)
			stopClicked();
	});

	RefreshSizes();
	RefreshIcons();
	SetControls(false, false, QString());
}

QSize PreviewMediaBar::sizeHint() const
{
	return {420, view->sizeHint().height() + 1};
}

void PreviewMediaBar::SetControls(bool enabled, bool isPlaying, const QString &lockReason)
{
	if (isPlaying != playing) {
		playing = isPlaying;
		RefreshIcons();
		const QString tip = QTStr(playing ? "Preview.Pause" : "Preview.Play");
		playPauseButton->setToolTip(tip);
		playPauseButton->setAccessibleName(tip);
	}

	if (enabled != controlsEnabled) {
		controlsEnabled = enabled;
		for (QToolButton *button : {playPauseButton, restartButton, stopButton})
			button->setEnabled(enabled);
	}

	if (lockReason != currentLockReason) {
		currentLockReason = lockReason;
		setToolTip(lockReason);
	}
}

void PreviewMediaBar::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	QColor line = palette().color(QPalette::WindowText);
	line.setAlphaF(0.12f);
	painter.fillRect(QRect(0, 0, width(), 1), line);
}

void PreviewMediaBar::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
		RefreshIcons();
	if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
		RefreshSizes();
	QWidget::changeEvent(event);
}

void PreviewMediaBar::RefreshIcons()
{
	const QColor color = palette().color(QPalette::ButtonText);
	playPauseButton->setIcon(MakeMediaIcon(playing ? MediaIcon::Pause : MediaIcon::Play, color));
	restartButton->setIcon(MakeMediaIcon(MediaIcon::Restart, color));
	stopButton->setIcon(MakeMediaIcon(MediaIcon::Stop, color));
}

void PreviewMediaBar::RefreshSizes()
{
	/* buttons fit the bar, so Preview and Program bars stay the same height */
	const int side = std::max(18, view->sizeHint().height() - 6);
	for (QToolButton *button : {playPauseButton, restartButton, stopButton})
		button->setFixedSize(side, side);
	updateGeometry();
}

/* ------------------------------------------------------------------------ */

MediaCountdownController::MediaCountdownController(QMainWindow *main_) : QObject(main_), main(main_)
{
	clock.start();

	timer.setInterval(kTickMs);
	connect(&timer, &QTimer::timeout, this, &MediaCountdownController::Tick);

	seekTimer.setSingleShot(true);
	seekTimer.setInterval(kSeekThrottleMs);
	connect(&seekTimer, &QTimer::timeout, this, &MediaCountdownController::ApplyPendingSeek);

	UpdatePlacement();
}

MediaCountdownController::~MediaCountdownController()
{
	timer.stop();
	seekTimer.stop();
	delete regularView.data();
	delete studioView.data();
	delete previewBar.data();
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

	const PluginSettings &cfg = Settings();
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
		regularView->setVisible(cfg.countdownEnabled && !studio);

	/* The program widget is rebuilt every time studio mode is enabled, and
	 * our bar is destroyed with it when studio mode is disabled. */
	if (studio && cfg.countdownEnabled && !studioView) {
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
		studioView->setVisible(cfg.countdownEnabled && studio);

	/* Below the Preview display (and its zoom/scroll row) */
	if (!previewBar) {
		auto *layout = main->findChild<QBoxLayout *>(QStringLiteral("previewTextLayout"));
		QWidget *container = layout ? layout->parentWidget() : nullptr;
		if (layout && container) {
			previewBar = new PreviewMediaBar(container);
			layout->addWidget(previewBar);

			previewBar->playPauseClicked = [this]() { PreviewPlayPause(); };
			previewBar->restartClicked = [this]() { PreviewRestart(); };
			previewBar->stopClicked = [this]() { PreviewStop(); };
			previewBar->View()->seekRequested = [this](double fraction, bool done) {
				PreviewSeek(fraction, done);
			};
		} else {
			plog(LOG_WARNING, "Preview layout not found, preview media controls unavailable");
		}
	}
	if (previewBar)
		previewBar->setVisible(cfg.previewControlsEnabled && studio);

	const bool shown = (regularView && !regularView->isHidden()) || (studioView && !studioView->isHidden()) ||
			   (previewBar && !previewBar->isHidden());
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
	if ((regularView && !regularView->isHidden()) || (studioView && !studioView->isHidden()))
		UpdateProgram();
	if (previewBar && !previewBar->isHidden())
		UpdatePreview();
}

void MediaCountdownController::UpdateProgram()
{
	const PluginSettings &cfg = Settings();

	OBSSourceAutoRelease root = ProgramRoot();
	std::vector<MediaInfo> media = CollectMedia(root, false);
	SortForProgram(media);

	CountdownState state;
	state.showElapsed = cfg.showElapsed;
	state.idleText = QTStr("Countdown.Idle");

	if (!media.empty()) {
		const MediaInfo &primary = media.front();
		state.hasMedia = true;
		state.name = primary.name;
		state.paused = primary.state == OBS_MEDIA_STATE_PAUSED;
		state.status = state.paused ? QTStr("Countdown.Paused") : QString();
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

		if (state.others > 0)
			state.tooltip = MediaListTooltip("Countdown.AllMedia", media, false);
	}

	if (regularView && !regularView->isHidden())
		regularView->SetState(state);
	if (studioView && !studioView->isHidden())
		studioView->SetState(state);
}

void MediaCountdownController::UpdatePreview()
{
	if (!previewBar)
		return;

	const PluginSettings &cfg = Settings();

	OBSSourceAutoRelease scene = obs_frontend_get_current_preview_scene();
	std::vector<MediaInfo> media = CollectMedia(scene, true);
	SortForPreview(media);

	CountdownState state;
	state.showElapsed = cfg.showElapsed;
	state.idleText = QTStr("Preview.Idle");

	if (media.empty()) {
		previewTarget = nullptr;
		previewPlaying = false;
		resumeDeadline = 0;
		previewBar->View()->SetState(state);
		previewBar->SetControls(false, false, QString());
		return;
	}

	const MediaInfo &primary = media.front();
	const qint64 now = clock.elapsed();

	OBSWeakSourceAutoRelease weak = obs_source_get_weak_source(primary.source);
	if (weak.Get() != previewTarget.Get()) {
		previewTarget = weak.Get();
		previewLastTime = primary.elapsedMs;
		previewLastAdvance = now;
		resumeDeadline = 0;
	}

	if (primary.elapsedMs != previewLastTime) {
		previewLastTime = primary.elapsedMs;
		previewLastAdvance = now;
	}

	const bool stalled = primary.state == OBS_MEDIA_STATE_PLAYING && primary.durationMs > 0 &&
			     now - previewLastAdvance > kStallMs;
	const bool playing = IsBusy(primary.state) && primary.state != OBS_MEDIA_STATE_PAUSED && !stalled;
	const bool paused = primary.state == OBS_MEDIA_STATE_PAUSED;

	/* resuming media that was stopped behind OBS's back does nothing; fall
	 * back to a restart */
	if (resumeDeadline && now >= resumeDeadline) {
		resumeDeadline = 0;
		if (!playing && !primary.onAir)
			obs_source_media_restart(primary.source);
	}

	state.hasMedia = true;
	state.name = primary.name;
	state.looping = primary.looping;
	state.onAir = primary.onAir;
	state.onAirBlinkOn = primary.onAir && TallyBlinkOn();
	state.paused = paused;
	state.stopped = !playing && !paused;
	if (paused)
		state.status = QTStr("Countdown.Paused");
	else if (state.stopped)
		state.status = QTStr(primary.state == OBS_MEDIA_STATE_ENDED   ? "Preview.Ended"
				     : primary.state == OBS_MEDIA_STATE_ERROR ? "Preview.Error"
									      : "Preview.Stopped");
	state.hasDuration = primary.durationMs > 0;
	state.elapsedMs = primary.elapsedMs;
	state.durationMs = primary.durationMs;
	state.remainingMs = primary.remainingMs;
	state.seekable = !primary.onAir && (playing || paused) && state.hasDuration;
	state.others = (int)media.size() - 1;
	if (state.others > 0)
		state.tooltip = MediaListTooltip("Preview.AllMedia", media, true);

	previewPlaying = playing;
	previewBar->View()->SetState(state);
	previewBar->SetControls(!primary.onAir, playing, primary.onAir ? QTStr("Preview.OnAirLocked") : QString());
}

OBSSourceAutoRelease MediaCountdownController::PreviewTarget() const
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(previewTarget);

	/* never touch media that is live in Program */
	if (source && obs_source_active(source))
		return nullptr;
	return source;
}

void MediaCountdownController::PreviewPlayPause()
{
	OBSSourceAutoRelease source = PreviewTarget();
	if (!source)
		return;

	const qint64 now = clock.elapsed();
	if (previewPlaying) {
		obs_source_media_play_pause(source, true);
	} else if (obs_source_media_get_state(source) == OBS_MEDIA_STATE_PAUSED) {
		obs_source_media_play_pause(source, false);
		/* checked once stall detection has had time to notice */
		resumeDeadline = now + kStallMs + 2 * kTickMs;
	} else {
		/* not started yet, stopped or ended: play from the beginning */
		obs_source_media_restart(source);
	}

	previewLastAdvance = now;
	UpdatePreview();
}

void MediaCountdownController::PreviewRestart()
{
	OBSSourceAutoRelease source = PreviewTarget();
	if (!source)
		return;

	obs_source_media_restart(source);
	previewLastAdvance = clock.elapsed();
	resumeDeadline = 0;
	UpdatePreview();
}

void MediaCountdownController::PreviewStop()
{
	OBSSourceAutoRelease source = PreviewTarget();
	if (!source)
		return;

	obs_source_media_stop(source);
	resumeDeadline = 0;
	UpdatePreview();
}

void MediaCountdownController::PreviewSeek(double fraction, bool done)
{
	OBSSourceAutoRelease source = PreviewTarget();
	if (!source)
		return;

	const int64_t duration = obs_source_media_get_duration(source);
	if (duration <= 0)
		return;

	pendingSeekMs = (qint64)(fraction * (double)duration);
	if (done) {
		seekTimer.stop();
		ApplyPendingSeek();
	} else if (!seekTimer.isActive()) {
		seekTimer.start();
	}
}

void MediaCountdownController::ApplyPendingSeek()
{
	if (pendingSeekMs < 0)
		return;

	OBSSourceAutoRelease source = PreviewTarget();
	if (source) {
		obs_source_media_set_time(source, pendingSeekMs);
		previewLastAdvance = clock.elapsed();
	}
	pendingSeekMs = -1;
}
