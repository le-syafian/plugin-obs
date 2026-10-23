#include "scene-colors.hpp"
#include "plugin-settings.hpp"
#include "plugin-support.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QAbstractItemDelegate>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QHelpEvent>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {

/* Item data roles that hand color and tally to the delegate. OBS itself only
 * uses Qt::UserRole and Qt::UserRole + 1 on scene list items. */
constexpr int kSceneColorRole = Qt::UserRole + 0x5CC;
constexpr int kTallyRole = Qt::UserRole + 0x5CD;

constexpr int kTallyPreview = 1;
constexpr int kTallyProgram = 2;

constexpr int kTallyPollMs = 150;
constexpr int kStripeWidth = 5;

/* Key inside the scene's private settings, saved with the scene collection */
constexpr const char *kColorKey = "scene-color-countdown.color";

struct PresetColor {
	const char *textKey;
	QRgb rgb;
};

constexpr PresetColor kPresets[] = {
	{"SceneColor.Red", 0xd64545},    {"SceneColor.Orange", 0xe07b39}, {"SceneColor.Yellow", 0xe5b53a},
	{"SceneColor.Green", 0x3fa34d},  {"SceneColor.Teal", 0x2a9d8f},   {"SceneColor.Blue", 0x3d7dd8},
	{"SceneColor.Purple", 0x8a5cd1}, {"SceneColor.Pink", 0xd35d9a},   {"SceneColor.Gray", 0x6b7280},
};

OBSSourceAutoRelease FindScene(const QString &name)
{
	if (name.isEmpty())
		return nullptr;

	OBSSourceAutoRelease source = obs_get_source_by_name(name.toUtf8().constData());
	if (source && !obs_source_is_scene(source))
		return nullptr;
	return source;
}

QColor ReadSceneColor(obs_source_t *scene)
{
	OBSDataAutoRelease priv = obs_source_get_private_settings(scene);
	const char *value = obs_data_get_string(priv, kColorKey);
	if (!value || !*value)
		return {};

	const QColor color = QColor::fromString(QLatin1StringView(value));
	return color.isValid() ? color : QColor();
}

void WriteSceneColor(obs_source_t *scene, const QColor &color)
{
	OBSDataAutoRelease priv = obs_source_get_private_settings(scene);
	if (color.isValid())
		obs_data_set_string(priv, kColorKey, color.name(QColor::HexRgb).toUtf8().constData());
	else
		obs_data_erase(priv, kColorKey);
}

QString SourceName(obs_source_t *source)
{
	return QString::fromUtf8(obs_source_get_name(source));
}

/* Scenes really visible on the output: the current one and, during a
 * transition, also the incoming one. In studio mode with "Duplicate Scene"
 * the output uses private copies that keep the original scene names. */
QStringList ProgramScenes()
{
	QStringList names;

	OBSSourceAutoRelease output = obs_get_output_source(0);
	if (output && obs_source_get_type(output) == OBS_SOURCE_TYPE_TRANSITION) {
		for (obs_transition_target target : {OBS_TRANSITION_SOURCE_A, OBS_TRANSITION_SOURCE_B}) {
			OBSSourceAutoRelease source = obs_transition_get_source(output, target);
			if (source)
				names << SourceName(source);
		}
	} else if (output) {
		names << SourceName(output);
	} else {
		OBSSourceAutoRelease current = obs_frontend_get_current_scene();
		if (current)
			names << SourceName(current);
	}

	return names;
}

QString PreviewScene()
{
	if (!obs_frontend_preview_program_mode_active())
		return {};

	OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
	return preview ? SourceName(preview) : QString();
}

/* Blinking dot for ON AIR, eye for PREVIEW, on the right of the row */
struct TallySymbols {
	qreal dotRadius = 0;
	QSizeF eye;
	int width = 0; /* 0: no room, the row only gets frame and tint */
};

TallySymbols MeasureSymbols(const QRect &rect, int tally)
{
	TallySymbols symbols;
	if (!tally)
		return symbols;

	symbols.dotRadius = std::clamp(rect.height() * 0.16, 3.5, 5.0);
	const qreal eyeWidth = std::clamp(rect.height() * 0.62, 12.0, 16.0);
	symbols.eye = QSizeF(eyeWidth, eyeWidth * 0.62);

	qreal width = 0;
	if (tally & kTallyProgram)
		width += 2 * (symbols.dotRadius + 2.5);
	if (tally & kTallyPreview)
		width += eyeWidth;
	if ((tally & kTallyProgram) && (tally & kTallyPreview))
		width += 4;

	symbols.width = (int)std::ceil(width);
	if (rect.width() < symbols.width + 58)
		symbols.width = 0;
	return symbols;
}

/*
 * Wraps OBS's own delegate: paints colored and tallied scenes itself and
 * forwards everything else. OBS listens to the original delegate's
 * closeEditor() to apply scene renames, so editing must keep going through
 * that delegate; this class only relays its signals to the view.
 */
class SceneColorDelegate : public QAbstractItemDelegate {
public:
	SceneColorDelegate(QAbstractItemDelegate *base_, QListWidget *view_)
		: QAbstractItemDelegate(view_),
		  base(base_),
		  view(view_)
	{
		connect(base_, &QAbstractItemDelegate::commitData, this, &QAbstractItemDelegate::commitData);
		connect(base_, &QAbstractItemDelegate::closeEditor, this, &QAbstractItemDelegate::closeEditor);
		connect(base_, &QAbstractItemDelegate::sizeHintChanged, this, &QAbstractItemDelegate::sizeHintChanged);
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		if (!base)
			return;

		const PluginSettings &cfg = Settings();
		const QColor sceneColor = index.data(kSceneColorRole).value<QColor>();
		const int tally = cfg.tallyEnabled ? index.data(kTallyRole).toInt() : 0;

		if (!sceneColor.isValid() && !tally) {
			base->paint(painter, option, index);
			return;
		}

		const bool onAir = tally & kTallyProgram;
		const QColor tallyColor = onAir ? cfg.tallyProgramColor : cfg.tallyPreviewColor;
		const bool fillTally = tally && cfg.tallyStyle == TallyStyle::Fill;

		/* room for the ON AIR dot / PREVIEW eye on the right */
		const TallySymbols symbols = MeasureSymbols(option.rect, tally);
		const int reserved = symbols.width ? symbols.width + 10 : 0;

		if (fillTally) {
			PaintFill(painter, option, index, tallyColor, reserved, true);
			if (sceneColor.isValid())
				PaintStripe(painter, option.rect, sceneColor);
		} else if (sceneColor.isValid() && cfg.colorStyle == SceneColorStyle::Fill) {
			PaintFill(painter, option, index, sceneColor, reserved, tally != 0);
		} else {
			/* keep the theme look: tint, OBS's own painting, optional stripe */
			if (tally)
				PaintTint(painter, option.rect, tallyColor);

			QStyleOptionViewItem themed(option);
			if (sceneColor.isValid())
				themed.rect.setLeft(option.rect.left() + kStripeWidth + 3);
			themed.rect.setRight(option.rect.right() - reserved);
			base->paint(painter, themed, index);

			if (sceneColor.isValid())
				PaintStripe(painter, option.rect, sceneColor);
		}

		if (tally)
			PaintTally(painter, option.rect, tally, fillTally, symbols);
	}

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		return base ? base->sizeHint(option, index) : QSize();
	}

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
			      const QModelIndex &index) const override
	{
		return base ? base->createEditor(parent, option, index) : nullptr;
	}

	void destroyEditor(QWidget *editor, const QModelIndex &index) const override
	{
		if (base)
			base->destroyEditor(editor, index);
		else
			QAbstractItemDelegate::destroyEditor(editor, index);
	}

	void setEditorData(QWidget *editor, const QModelIndex &index) const override
	{
		if (base)
			base->setEditorData(editor, index);
	}

	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
	{
		if (base)
			base->setModelData(editor, model, index);
	}

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
				  const QModelIndex &index) const override
	{
		if (base)
			base->updateEditorGeometry(editor, option, index);
	}

	bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
			 const QModelIndex &index) override
	{
		return base && base->editorEvent(event, model, option, index);
	}

	bool helpEvent(QHelpEvent *event, QAbstractItemView *itemView, const QStyleOptionViewItem &option,
		       const QModelIndex &index) override
	{
		/* the symbols have no text, so say what they mean on hover */
		const int tally = Settings().tallyEnabled ? index.data(kTallyRole).toInt() : 0;
		if (event && itemView && event->type() == QEvent::ToolTip && tally &&
		    !index.data(Qt::ToolTipRole).isValid()) {
			QStringList parts;
			if (tally & kTallyProgram)
				parts << QTStr("Tally.Program");
			if (tally & kTallyPreview)
				parts << QTStr("Tally.Preview");
			QToolTip::showText(event->globalPos(), parts.join(QStringLiteral(" · ")),
					   itemView->viewport(), option.rect);
			return true;
		}

		return base && base->helpEvent(event, itemView, option, index);
	}

	QList<int> paintingRoles() const override
	{
		QList<int> roles = base ? base->paintingRoles() : QList<int>();
		if (!roles.isEmpty())
			roles << kSceneColorRole << kTallyRole;
		return roles;
	}

	/* The view installs the active delegate as event filter on the rename
	 * editor; the original delegate handles Enter/Esc/focus-out there. */
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		return base && static_cast<QObject *>(base.data())->eventFilter(watched, event);
	}

private:
	void PaintFill(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
		       const QColor &color, int reservedRight, bool tallied) const
	{
		const bool selected = option.state & QStyle::State_Selected;
		const bool hovered = option.state & QStyle::State_MouseOver;
		const bool grid = view && view->viewMode() == QListView::IconMode;
		const QColor textColor = ContrastingText(color);

		painter->save();
		painter->setClipRect(option.rect);
		painter->setRenderHint(QPainter::Antialiasing, true);

		const QRectF box = QRectF(option.rect).adjusted(1, 1, -1, -1);
		painter->setPen(Qt::NoPen);
		painter->setBrush(hovered && !selected ? color.lighter(115) : color);
		painter->drawRoundedRect(box, 4, 4);

		/* keep the color, mark the selection with an outline (a tallied row
		 * already gets a frame, and the selection is always tallied) */
		if (selected && !tallied) {
			painter->setPen(QPen(textColor, 1.5));
			painter->setBrush(Qt::NoBrush);
			painter->drawRoundedRect(box.adjusted(2.5, 2.5, -2.5, -2.5), 2.5, 2.5);
		}

		QFont font = option.font;
		font.setBold(selected);
		painter->setFont(font);
		painter->setPen(textColor);

		const QString text = index.data(Qt::DisplayRole).toString();
		const QRect textRect = option.rect.adjusted(8, 2, -8 - reservedRight, -2);
		if (grid) {
			painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
		} else {
			const QString elided =
				painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
			painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);
		}

		painter->restore();
	}

	static void PaintStripe(QPainter *painter, const QRect &rect, const QColor &color)
	{
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setPen(Qt::NoPen);
		painter->setBrush(color);
		painter->drawRoundedRect(QRectF(rect.left() + 1.5, rect.top() + 2.5, kStripeWidth, rect.height() - 5), 2,
					 2);
		painter->restore();
	}

	static void PaintTint(QPainter *painter, const QRect &rect, QColor color)
	{
		color.setAlphaF(0.3);
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setPen(Qt::NoPen);
		painter->setBrush(color);
		painter->drawRoundedRect(QRectF(rect).adjusted(1, 1, -1, -1), 4, 4);
		painter->restore();
	}

	static void PaintTally(QPainter *painter, const QRect &rect, int tally, bool filled,
			       const TallySymbols &symbols)
	{
		const PluginSettings &cfg = Settings();
		const bool onAir = tally & kTallyProgram;
		const bool inPreview = tally & kTallyPreview;
		const QColor color = onAir ? cfg.tallyProgramColor : cfg.tallyPreviewColor;
		const QRectF box = QRectF(rect).adjusted(1.5, 1.5, -1.5, -1.5);

		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setBrush(Qt::NoBrush);

		if (!filled) {
			painter->setPen(QPen(color, 2));
			painter->drawRoundedRect(box, 4, 4);
		}

		/* ON AIR and also in Preview: inner frame in the preview color */
		if (onAir && inPreview) {
			painter->setPen(QPen(cfg.tallyPreviewColor, 1.5));
			painter->drawRoundedRect(box.adjusted(2, 2, -2, -2), 2.5, 2.5);
		}

		painter->restore();

		if (!symbols.width)
			return;

		/* right to left: the ON AIR dot, then the PREVIEW eye */
		qreal right = rect.right() - 6;
		const qreal centerY = rect.center().y() + 0.5;
		if (onAir) {
			const qreal dotBox = 2 * (symbols.dotRadius + 2.5);
			DrawOnAirDot(*painter, QPointF(right - dotBox / 2, centerY), symbols.dotRadius,
				     filled ? QColor(Qt::white) : cfg.tallyProgramColor, TallyBlinkOn());
			right -= dotBox + 4;
		}
		if (inPreview) {
			const QRectF eye(right - symbols.eye.width(), centerY - symbols.eye.height() / 2,
					 symbols.eye.width(), symbols.eye.height());
			DrawPreviewEye(*painter, eye, filled ? QColor(Qt::white) : cfg.tallyPreviewColor);
		}
	}

	QPointer<QAbstractItemDelegate> base;
	QPointer<QListWidget> view;
};

} // namespace

SceneColorManager::SceneColorManager(QMainWindow *main) : QObject(main)
{
	refreshTimer.setSingleShot(true);
	refreshTimer.setInterval(0);
	connect(&refreshTimer, &QTimer::timeout, this, &SceneColorManager::RefreshNow);

	/* the output's sources change without a frontend event when a
	 * transition starts, so tally is polled lightly */
	tallyTimer.setInterval(kTallyPollMs);
	connect(&tallyTimer, &QTimer::timeout, this, &SceneColorManager::RefreshTally);

	blinkTimer.setSingleShot(true);
	connect(&blinkTimer, &QTimer::timeout, this, &SceneColorManager::BlinkTick);

	list = main ? main->findChild<QListWidget *>(QStringLiteral("scenes")) : nullptr;
	if (!list) {
		plog(LOG_WARNING, "Scenes list not found, scene colors are disabled");
		return;
	}

	originalDelegate = list->itemDelegate();
	if (originalDelegate) {
		colorDelegate = new SceneColorDelegate(originalDelegate, list);
		list->setItemDelegate(colorDelegate);
	} else {
		plog(LOG_WARNING, "Scenes list has no item delegate, colors will not be painted");
	}

	list->installEventFilter(this);
	list->viewport()->installEventFilter(this);

	QAbstractItemModel *model = list->model();
	connect(model, &QAbstractItemModel::rowsInserted, this, &SceneColorManager::ScheduleRefresh);
	connect(model, &QAbstractItemModel::modelReset, this, &SceneColorManager::ScheduleRefresh);
	connect(model, &QAbstractItemModel::dataChanged, this,
		[this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
			if (applying)
				return;
			/* a rename changes the display text, which is how we look the scene up */
			if (roles.isEmpty() || roles.contains(Qt::DisplayRole) || roles.contains(Qt::EditRole))
				ScheduleRefresh();
		});

	ApplySettings();
	plog(LOG_INFO, "Scene colors and tally attached to the Scenes dock");
}

SceneColorManager::~SceneColorManager()
{
	DisarmMenuHook();
	refreshTimer.stop();
	tallyTimer.stop();
	blinkTimer.stop();

	if (list) {
		QObject::disconnect(list->model(), nullptr, this, nullptr);
		list->removeEventFilter(this);
		list->viewport()->removeEventFilter(this);

		if (originalDelegate && list->itemDelegate() == colorDelegate.data())
			list->setItemDelegate(originalDelegate);

		for (int i = 0; i < list->count(); i++) {
			list->item(i)->setData(kSceneColorRole, QVariant());
			list->item(i)->setData(kTallyRole, QVariant());
		}
	}

	delete colorDelegate.data();
}

void SceneColorManager::ScheduleRefresh()
{
	refreshTimer.start();
}

void SceneColorManager::RefreshNow()
{
	refreshTimer.stop();
	if (!list)
		return;

	applying = true;
	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		OBSSourceAutoRelease scene = FindScene(item->text());
		const QColor color = scene ? ReadSceneColor(scene) : QColor();
		const QVariant value = color.isValid() ? QVariant::fromValue(color) : QVariant();
		if (item->data(kSceneColorRole) != value)
			item->setData(kSceneColorRole, value);
	}
	applying = false;

	programScenes = ProgramScenes();
	previewScene = PreviewScene();
	ApplyTally();
}

void SceneColorManager::RefreshTally()
{
	if (!list || !Settings().tallyEnabled)
		return;

	QStringList program = ProgramScenes();
	QString preview = PreviewScene();
	if (program == programScenes && preview == previewScene)
		return;

	programScenes = std::move(program);
	previewScene = std::move(preview);
	ApplyTally();
}

void SceneColorManager::ApplyTally()
{
	if (!list)
		return;

	applying = true;
	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		const QString name = item->text();

		int flags = 0;
		if (!previewScene.isEmpty() && name == previewScene)
			flags |= kTallyPreview;
		if (programScenes.contains(name))
			flags |= kTallyProgram;

		const QVariant value = flags ? QVariant(flags) : QVariant();
		if (item->data(kTallyRole) != value)
			item->setData(kTallyRole, value);
	}
	applying = false;
}

void SceneColorManager::ApplySettings()
{
	if (Settings().tallyEnabled) {
		if (!tallyTimer.isActive())
			tallyTimer.start();
		if (!blinkTimer.isActive())
			ScheduleBlink();
	} else {
		tallyTimer.stop();
		blinkTimer.stop();
	}

	RefreshNow();
	Repaint();
}

/* Fire right after each blink boundary, so the dots in the Scenes dock and
 * the bars under Preview/Program blink together */
void SceneColorManager::ScheduleBlink()
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	blinkTimer.start(int(kTallyBlinkMs - now % kTallyBlinkMs) + 2);
}

void SceneColorManager::BlinkTick()
{
	if (!list || !Settings().tallyEnabled)
		return;

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		if (item->data(kTallyRole).toInt() & kTallyProgram)
			list->viewport()->update(list->visualItemRect(item));
	}
	ScheduleBlink();
}

void SceneColorManager::Repaint()
{
	if (list)
		list->viewport()->update();
}

bool SceneColorManager::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::ContextMenu && list &&
	    (watched == list.data() || watched == list->viewport())) {
		/* OBS builds its scene menu right after this event; remember which
		 * scene it is for and catch that menu before it is shown. */
		auto *menuEvent = static_cast<QContextMenuEvent *>(event);
		QListWidgetItem *item = nullptr;
		if (menuEvent->reason() == QContextMenuEvent::Mouse) {
			QPoint pos = menuEvent->pos();
			if (watched == list.data())
				pos = list->viewport()->mapFrom(list, pos);
			item = list->itemAt(pos);
		} else {
			item = list->currentItem();
		}

		if (item)
			ArmMenuHook(item->text());
		else
			DisarmMenuHook();
		return false;
	}

	if (menuHookArmed && event->type() == QEvent::Polish) {
		auto *menu = qobject_cast<QMenu *>(watched);
		if (menu && !qobject_cast<QMenu *>(menu->parentWidget())) {
			const QString sceneName = pendingSceneName;
			DisarmMenuHook();

			QPointer<QMenu> guard(menu);
			connect(
				menu, &QMenu::aboutToShow, this,
				[this, guard, sceneName]() {
					if (guard)
						InjectMenu(guard, sceneName);
				},
				Qt::SingleShotConnection);
		}
	}

	return QObject::eventFilter(watched, event);
}

void SceneColorManager::ArmMenuHook(const QString &sceneName)
{
	pendingSceneName = sceneName;
	if (!menuHookArmed) {
		qApp->installEventFilter(this);
		menuHookArmed = true;
	}

	/* If no menu shows up synchronously, stop watching on the next loop
	 * iteration so we never touch unrelated menus. */
	QTimer::singleShot(0, this, [this]() { DisarmMenuHook(); });
}

void SceneColorManager::DisarmMenuHook()
{
	if (menuHookArmed) {
		qApp->removeEventFilter(this);
		menuHookArmed = false;
	}
	pendingSceneName.clear();
}

void SceneColorManager::InjectMenu(QMenu *menu, const QString &sceneName)
{
	OBSSourceAutoRelease scene = FindScene(sceneName);
	if (!scene)
		return;

	const QColor current = ReadSceneColor(scene);

	auto *colorMenu = new QMenu(QTStr("SceneColor.Menu"), menu);
	if (current.isValid())
		colorMenu->setIcon(ColorSwatchIcon(current));

	auto *group = new QActionGroup(colorMenu);
	group->setExclusive(true);

	bool isPreset = false;
	for (const PresetColor &preset : kPresets) {
		const QColor color(preset.rgb);
		QAction *action = colorMenu->addAction(ColorSwatchIcon(color), QTStr(preset.textKey));
		action->setCheckable(true);
		action->setChecked(current == color);
		isPreset = isPreset || current == color;
		group->addAction(action);
		connect(action, &QAction::triggered, this,
			[this, sceneName, color]() { SetSceneColor(sceneName, color); });
	}

	colorMenu->addSeparator();

	QAction *custom = colorMenu->addAction(QTStr("SceneColor.Custom"));
	if (current.isValid() && !isPreset) {
		custom->setIcon(ColorSwatchIcon(current));
		custom->setCheckable(true);
		custom->setChecked(true);
		group->addAction(custom);
	}
	connect(custom, &QAction::triggered, this, [this, sceneName]() {
		/* open the dialog after OBS's menu has fully closed */
		QTimer::singleShot(0, this, [this, sceneName]() { PickCustomColor(sceneName); });
	});

	QAction *clear = colorMenu->addAction(QTStr("SceneColor.Clear"));
	clear->setEnabled(current.isValid());
	connect(clear, &QAction::triggered, this, [this, sceneName]() { SetSceneColor(sceneName, QColor()); });

	/* Put it under OBS's Rename/Remove entries when we can find them */
	const QList<QAction *> actions = menu->actions();
	QAction *before = nullptr;
	bool foundRemove = false;
	for (qsizetype i = 0; i < actions.size(); i++) {
		if (actions[i]->objectName() == QLatin1String("actionRemoveScene")) {
			before = i + 1 < actions.size() ? actions[i + 1] : nullptr;
			foundRemove = true;
			break;
		}
	}

	if (foundRemove) {
		menu->insertMenu(before, colorMenu);
	} else {
		menu->addSeparator();
		menu->addMenu(colorMenu);
	}
}

void SceneColorManager::SetSceneColor(const QString &sceneName, const QColor &color)
{
	OBSSourceAutoRelease scene = FindScene(sceneName);
	if (!scene)
		return;

	WriteSceneColor(scene, color);
	RefreshNow();

	/* private settings are not tracked by OBS's change detection */
	obs_frontend_save();
}

void SceneColorManager::PickCustomColor(const QString &sceneName)
{
	OBSSourceAutoRelease scene = FindScene(sceneName);
	if (!scene)
		return;

	QColor initial = ReadSceneColor(scene);
	if (!initial.isValid())
		initial = QColor(kPresets[5].rgb);

	QWidget *parent = list ? list->window() : nullptr;
	const QColor color = QColorDialog::getColor(initial, parent, QTStr("SceneColor.DialogTitle"),
						    QColorDialog::DontUseNativeDialog);
	if (color.isValid())
		SetSceneColor(sceneName, color);
}
