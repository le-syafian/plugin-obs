#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

class QAbstractItemDelegate;
class QListWidget;
class QMainWindow;
class QMenu;

/*
 * Lets every scene in the Scenes dock carry a color, and marks the scenes
 * that are ON AIR (Program) and in PREVIEW (studio mode) with tally colors.
 *
 * - The color is stored in the scene's private settings, so it is saved
 *   with the scene collection.
 * - OBS's own right-click menu on the Scenes list gets a "Scene Color"
 *   submenu (injected right before OBS shows the menu).
 * - Tally follows what is really on the output, so during a transition
 *   both the outgoing and the incoming scene are ON AIR.
 * - A proxy item delegate paints all of it; everything else (renaming,
 *   size hints) is forwarded to OBS's original delegate.
 */
class SceneColorManager : public QObject {
public:
	explicit SceneColorManager(QMainWindow *main);
	~SceneColorManager() override;

	void ScheduleRefresh();
	void RefreshNow();
	void RefreshTally();
	void ApplySettings();
	void Repaint();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void ApplyTally();
	void ScheduleBlink();
	void BlinkTick();
	void ArmMenuHook(const QString &sceneName);
	void DisarmMenuHook();
	void InjectMenu(QMenu *menu, const QString &sceneName);
	void SetSceneColor(const QString &sceneName, const QColor &color);
	void PickCustomColor(const QString &sceneName);

	QPointer<QListWidget> list;
	QPointer<QAbstractItemDelegate> originalDelegate;
	QPointer<QAbstractItemDelegate> colorDelegate;
	QTimer refreshTimer;
	QTimer tallyTimer;
	QTimer blinkTimer;
	QStringList programScenes;
	QString previewScene;
	bool applying = false;
	bool menuHookArmed = false;
	QString pendingSceneName;
};
