#pragma once

#include <obs.hpp>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>

class QMainWindow;
class QToolButton;

struct CountdownState {
	bool hasMedia = false;
	QString name;
	QString status;       /* "Paused", "Stopped", ... shown after the name */
	bool paused = false;
	bool stopped = false; /* not playing: stopped, ended or not started yet */
	bool looping = false;
	bool onAir = false;   /* preview bar: this media is also live in Program */
	bool onAirBlinkOn = false;
	bool hasDuration = false;
	qint64 elapsedMs = 0;
	qint64 durationMs = 0;
	qint64 remainingMs = 0;
	int others = 0;       /* other media in the same scene */
	QString tooltip;
	QString idleText;
	int level = 0;        /* 0 normal, 1 warning, 2 critical */
	bool blinkOn = false;
	bool showElapsed = true;
	bool seekable = false;

	bool operator==(const CountdownState &other) const = default;
};

/* One bar: state icon, name, progress, elapsed/total and remaining time */
class CountdownView : public QWidget {
public:
	explicit CountdownView(QWidget *parent = nullptr);

	void SetState(const CountdownState &newState);
	void SetDrawSeparator(bool draw);

	/* Click/drag on the progress bar while seekable. fraction is 0..1 of
	 * the duration, done is true once the mouse is released. */
	std::function<void(double fraction, bool done)> seekRequested;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void changeEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QFont TimeFont() const;
	bool OverTrack(const QPointF &pos) const;
	double FractionAt(const QPointF &pos) const;

	CountdownState state;
	bool drawSeparator = true;
	QRectF trackRect; /* progress track as last painted, for seeking */
	bool dragging = false;
	double dragFraction = 0.0;
};

/* Play/pause, restart and stop buttons next to a countdown, used under the
 * Preview display in studio mode */
class PreviewMediaBar : public QWidget {
public:
	explicit PreviewMediaBar(QWidget *parent = nullptr);

	CountdownView *View() const { return view; }
	void SetControls(bool enabled, bool playing, const QString &lockReason);

	std::function<void()> playPauseClicked;
	std::function<void()> restartClicked;
	std::function<void()> stopClicked;

	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	void RefreshIcons();
	void RefreshSizes();

	QToolButton *playPauseButton = nullptr;
	QToolButton *restartButton = nullptr;
	QToolButton *stopButton = nullptr;
	CountdownView *view = nullptr;
	bool playing = false;
	bool controlsEnabled = true;
	QString currentLockReason;
};

/*
 * Feeds the bars from the media in the scenes that are on screen.
 *
 * Program: what is really on the output (through the output transition, so
 * nested scenes, groups and studio-mode scene duplication are covered).
 * - Regular mode: the bar sits under the canvas area (the preview is
 *   Program there), so it also stays visible when the preview is disabled.
 * - Studio mode: the bar sits under the Program display.
 *
 * Preview (studio mode only): a bar with transport buttons under the
 * Preview display, so media can be checked before it goes on air. Media
 * that is live in Program at the same time is never touched from here.
 */
class MediaCountdownController : public QObject {
public:
	explicit MediaCountdownController(QMainWindow *main);
	~MediaCountdownController() override;

	/* Call after studio mode toggles or settings change */
	void UpdatePlacement();
	void Tick();

private:
	void UpdateProgram();
	void UpdatePreview();
	QWidget *FindProgramContainer() const;

	OBSSourceAutoRelease PreviewTarget() const;
	void PreviewPlayPause();
	void PreviewRestart();
	void PreviewStop();
	void PreviewSeek(double fraction, bool done);
	void ApplyPendingSeek();

	QPointer<QMainWindow> main;
	QPointer<CountdownView> regularView;
	QPointer<CountdownView> studioView;
	QPointer<PreviewMediaBar> previewBar;
	QTimer timer;
	QTimer seekTimer;
	QElapsedTimer clock;

	/* the media the preview bar controls */
	OBSWeakSource previewTarget;
	bool previewPlaying = false;
	qint64 previewLastTime = -1;
	qint64 previewLastAdvance = 0;
	qint64 resumeDeadline = 0;
	qint64 pendingSeekMs = -1;
};
