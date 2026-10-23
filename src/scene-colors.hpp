#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QAbstractItemDelegate;
class QListWidget;
class QMainWindow;
class QMenu;

/*
 * Lets every scene in the Scenes dock carry a color.
 *
 * - The color is stored in the scene's private settings, so it is saved
 *   with the scene collection.
 * - OBS's own right-click menu on the Scenes list gets a "Scene Color"
 *   submenu (injected right before OBS shows the menu).
 * - A proxy item delegate paints the color; everything else (renaming,
 *   size hints) is forwarded to OBS's original delegate.
 */
class SceneColorManager : public QObject {
public:
	explicit SceneColorManager(QMainWindow *main);
	~SceneColorManager() override;

	void ScheduleRefresh();
	void RefreshNow();
	void Repaint();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void ArmMenuHook(const QString &sceneName);
	void DisarmMenuHook();
	void InjectMenu(QMenu *menu, const QString &sceneName);
	void SetSceneColor(const QString &sceneName, const QColor &color);
	void PickCustomColor(const QString &sceneName);

	QPointer<QListWidget> list;
	QPointer<QAbstractItemDelegate> originalDelegate;
	QPointer<QAbstractItemDelegate> colorDelegate;
	QTimer refreshTimer;
	bool applying = false;
	bool menuHookArmed = false;
	QString pendingSceneName;
};
