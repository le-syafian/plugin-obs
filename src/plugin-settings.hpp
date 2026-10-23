#pragma once

#include <functional>

class QWidget;

enum class SceneColorStyle {
	Fill = 0,   /* the whole row takes the scene color */
	Stripe = 1, /* a color bar on the left, theme look stays */
};

struct PluginSettings {
	bool countdownEnabled = true;
	int warnSeconds = 10;
	int criticalSeconds = 5;
	bool blinkCritical = true;
	bool showElapsed = true;
	SceneColorStyle colorStyle = SceneColorStyle::Fill;
};

PluginSettings &Settings();
void LoadSettings();
void SaveSettings();

/* Modal settings dialog. onApply runs after the user presses OK. */
void ShowSettingsDialog(QWidget *parent, const std::function<void()> &onApply);
