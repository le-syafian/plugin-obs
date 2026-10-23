#include "scene-colors.hpp"
#include "plugin-settings.hpp"
#include "plugin-support.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QAbstractItemDelegate>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

namespace {

/* Item data role that hands the color to the delegate. OBS itself only uses
 * Qt::UserRole and Qt::UserRole + 1 on scene list items. */
constexpr int kSceneColorRole = Qt::UserRole + 0x5CC;

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

QColor ContrastingText(const QColor &background)
{
	const double luma = 0.299 * background.redF() + 0.587 * background.greenF() + 0.114 * background.blueF();
	return luma > 0.6 ? QColor(24, 24, 24) : QColor(255, 255, 255);
}

QIcon SwatchIcon(const QColor &color)
{
	const qreal dpr = qApp->devicePixelRatio();
	QPixmap pixmap(QSize(16, 16) * dpr);
	pixmap.setDevicePixelRatio(dpr);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(QPen(color.darker(150), 1));
	painter.setBrush(color);
	painter.drawRoundedRect(QRectF(1.5, 1.5, 13, 13), 3, 3);
	return QIcon(pixmap);
}

/*
 * Wraps OBS's own delegate: paints colored scenes itself and forwards
 * everything else. OBS listens to the original delegate's closeEditor() to
 * apply scene renames, so editing must keep going through that delegate;
 * this class only relays its signals to the view.
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
		const QColor color = index.data(kSceneColorRole).value<QColor>();
		if (!color.isValid()) {
			if (base)
				base->paint(painter, option, index);
			return;
		}

		if (Settings().colorStyle == SceneColorStyle::Stripe)
			PaintStripe(painter, option, index, color);
		else
			PaintFill(painter, option, index, color);
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
		return base && base->helpEvent(event, itemView, option, index);
	}

	QList<int> paintingRoles() const override
	{
		QList<int> roles = base ? base->paintingRoles() : QList<int>();
		if (!roles.isEmpty())
			roles.append(kSceneColorRole);
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
		       const QColor &color) const
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

		if (selected) {
			/* keep the scene color, mark the selection with an outline */
			painter->setPen(QPen(textColor, 2));
			painter->setBrush(Qt::NoBrush);
			painter->drawRoundedRect(box.adjusted(1, 1, -1, -1), 3, 3);
		}

		QFont font = option.font;
		font.setBold(selected);
		painter->setFont(font);
		painter->setPen(textColor);

		const QString text = index.data(Qt::DisplayRole).toString();
		const QRect textRect = option.rect.adjusted(8, 2, -8, -2);
		if (grid) {
			painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
		} else {
			const QString elided =
				painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width());
			painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);
		}

		painter->restore();
	}

	void PaintStripe(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
			 const QColor &color) const
	{
		constexpr int stripeWidth = 5;

		QStyleOptionViewItem shifted(option);
		shifted.rect.setLeft(option.rect.left() + stripeWidth + 3);
		if (base)
			base->paint(painter, shifted, index);

		painter->save();
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setPen(Qt::NoPen);
		painter->setBrush(color);
		painter->drawRoundedRect(
			QRectF(option.rect.left() + 1, option.rect.top() + 2, stripeWidth, option.rect.height() - 4), 2,
			2);
		painter->restore();
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

	RefreshNow();
	plog(LOG_INFO, "Scene colors attached to the Scenes dock");
}

SceneColorManager::~SceneColorManager()
{
	DisarmMenuHook();
	refreshTimer.stop();

	if (list) {
		QObject::disconnect(list->model(), nullptr, this, nullptr);
		list->removeEventFilter(this);
		list->viewport()->removeEventFilter(this);

		if (originalDelegate && list->itemDelegate() == colorDelegate.data())
			list->setItemDelegate(originalDelegate);

		for (int i = 0; i < list->count(); i++)
			list->item(i)->setData(kSceneColorRole, QVariant());
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
		colorMenu->setIcon(SwatchIcon(current));

	auto *group = new QActionGroup(colorMenu);
	group->setExclusive(true);

	bool isPreset = false;
	for (const PresetColor &preset : kPresets) {
		const QColor color(preset.rgb);
		QAction *action = colorMenu->addAction(SwatchIcon(color), QTStr(preset.textKey));
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
		custom->setIcon(SwatchIcon(current));
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
