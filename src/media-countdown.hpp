#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

class QMainWindow;

struct CountdownState {
	bool hasMedia = false;
	QString name;
	bool paused = false;
	bool looping = false;
	bool hasDuration = false;
	qint64 elapsedMs = 0;
	qint64 durationMs = 0;
	qint64 remainingMs = 0;
	int others = 0;      /* other media playing in Program at the same time */
	QString tooltip;
	int level = 0;       /* 0 normal, 1 warning, 2 critical */
	bool blinkOn = false;
	bool showElapsed = true;

	bool operator==(const CountdownState &other) const = default;
};

/* The bar under the Program view: name, progress, elapsed/total, remaining */
class CountdownView : public QWidget {
public:
	explicit CountdownView(QWidget *parent = nullptr);

	void SetState(const CountdownState &newState);

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	QFont TimeFont() const;

	CountdownState state;
};

/*
 * Finds the media that is actually on the Program output (through the
 * output transition, so nested scenes, groups and studio-mode scene
 * duplication are covered) and feeds the countdown bars.
 *
 * Regular mode: the bar sits under the canvas area (the preview is Program
 * there), so it also stays visible when the preview is disabled.
 * Studio mode: the bar sits under the Program display.
 */
class MediaCountdownController : public QObject {
public:
	explicit MediaCountdownController(QMainWindow *main);
	~MediaCountdownController() override;

	/* Call after studio mode toggles or settings change */
	void UpdatePlacement();

private:
	void Tick();
	QWidget *FindProgramContainer() const;

	QPointer<QMainWindow> main;
	QPointer<CountdownView> regularView;
	QPointer<CountdownView> studioView;
	QTimer timer;
};
