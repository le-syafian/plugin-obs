#pragma once

#include <QColor>

#include <functional>

class QWidget;

enum class SceneColorStyle {
	Fill = 0,   /* the whole row takes the scene color */
	Stripe = 1, /* a color bar on the left, theme look stays */
};

enum class TallyStyle {
	Frame = 0, /* colored frame + ON AIR / PREVIEW label */
	Fill = 1,  /* the row takes the tally color */
};

struct PluginSettings {
	bool countdownEnabled = true;
	int warnSeconds = 10;
	int criticalSeconds = 5;
	bool blinkCritical = true;
	bool showElapsed = true;

	bool previewControlsEnabled = true;

	bool tallyEnabled = true;
	QColor tallyProgramColor = QColor(0xe5, 0x39, 0x35);
	QColor tallyPreviewColor = QColor(0x2e, 0x9e, 0x44);
	TallyStyle tallyStyle = TallyStyle::Frame;

	SceneColorStyle colorStyle = SceneColorStyle::Fill;
};

PluginSettings &Settings();
void LoadSettings();
void SaveSettings();

/* Modal settings dialog. onApply runs after the user presses OK. */
void ShowSettingsDialog(QWidget *parent, const std::function<void()> &onApply);
