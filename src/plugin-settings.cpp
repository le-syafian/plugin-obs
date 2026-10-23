#include "plugin-settings.hpp"
#include "plugin-support.hpp"

#include <util/platform.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

PluginSettings settings;

constexpr const char *kSettingsFile = "settings.json";

} // namespace

PluginSettings &Settings()
{
	return settings;
}

void LoadSettings()
{
	char *path = obs_module_config_path(kSettingsFile);
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!data)
		return; /* first run: keep defaults */

	const PluginSettings defaults;
	obs_data_set_default_bool(data, "countdown_enabled", defaults.countdownEnabled);
	obs_data_set_default_int(data, "warn_seconds", defaults.warnSeconds);
	obs_data_set_default_int(data, "critical_seconds", defaults.criticalSeconds);
	obs_data_set_default_bool(data, "blink_critical", defaults.blinkCritical);
	obs_data_set_default_bool(data, "show_elapsed", defaults.showElapsed);
	obs_data_set_default_int(data, "color_style", (int)defaults.colorStyle);

	settings.countdownEnabled = obs_data_get_bool(data, "countdown_enabled");
	settings.warnSeconds = (int)obs_data_get_int(data, "warn_seconds");
	settings.criticalSeconds = (int)obs_data_get_int(data, "critical_seconds");
	settings.blinkCritical = obs_data_get_bool(data, "blink_critical");
	settings.showElapsed = obs_data_get_bool(data, "show_elapsed");
	settings.colorStyle = obs_data_get_int(data, "color_style") == (int)SceneColorStyle::Stripe
				      ? SceneColorStyle::Stripe
				      : SceneColorStyle::Fill;

	if (settings.criticalSeconds > settings.warnSeconds)
		settings.warnSeconds = settings.criticalSeconds;

	obs_data_release(data);
}

void SaveSettings()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(kSettingsFile);
	if (!path)
		return;

	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "countdown_enabled", settings.countdownEnabled);
	obs_data_set_int(data, "warn_seconds", settings.warnSeconds);
	obs_data_set_int(data, "critical_seconds", settings.criticalSeconds);
	obs_data_set_bool(data, "blink_critical", settings.blinkCritical);
	obs_data_set_bool(data, "show_elapsed", settings.showElapsed);
	obs_data_set_int(data, "color_style", (int)settings.colorStyle);

	if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
		plog(LOG_WARNING, "Failed to save settings to %s", path);

	obs_data_release(data);
	bfree(path);
}

void ShowSettingsDialog(QWidget *parent, const std::function<void()> &onApply)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(QTStr("Settings.Title"));

	auto *countdownEnabled = new QCheckBox(QTStr("Settings.ShowCountdown"));
	auto *warn = new QSpinBox;
	auto *critical = new QSpinBox;
	auto *blink = new QCheckBox(QTStr("Settings.Blink"));
	auto *elapsed = new QCheckBox(QTStr("Settings.ShowElapsed"));
	auto *style = new QComboBox;

	for (QSpinBox *spin : {warn, critical}) {
		spin->setRange(1, 600);
		spin->setSuffix(QTStr("Settings.SecondsSuffix"));
	}
	style->addItem(QTStr("Settings.Style.Fill"), (int)SceneColorStyle::Fill);
	style->addItem(QTStr("Settings.Style.Stripe"), (int)SceneColorStyle::Stripe);

	countdownEnabled->setChecked(settings.countdownEnabled);
	warn->setValue(settings.warnSeconds);
	critical->setValue(settings.criticalSeconds);
	blink->setChecked(settings.blinkCritical);
	elapsed->setChecked(settings.showElapsed);
	style->setCurrentIndex(style->findData((int)settings.colorStyle));

	/* critical threshold can never be above the warning threshold */
	QObject::connect(warn, &QSpinBox::valueChanged, critical, [critical](int value) {
		if (critical->value() > value)
			critical->setValue(value);
	});
	QObject::connect(critical, &QSpinBox::valueChanged, warn, [warn](int value) {
		if (warn->value() < value)
			warn->setValue(value);
	});

	auto syncEnabled = [countdownEnabled, warn, critical, blink, elapsed]() {
		const bool on = countdownEnabled->isChecked();
		for (QWidget *widget : std::initializer_list<QWidget *>{warn, critical, blink, elapsed})
			widget->setEnabled(on);
	};
	QObject::connect(countdownEnabled, &QCheckBox::toggled, &dialog, syncEnabled);
	syncEnabled();

	auto *countdownGroup = new QGroupBox(QTStr("Settings.Countdown"));
	auto *countdownForm = new QFormLayout(countdownGroup);
	countdownForm->addRow(countdownEnabled);
	countdownForm->addRow(QTStr("Settings.WarnSeconds"), warn);
	countdownForm->addRow(QTStr("Settings.CriticalSeconds"), critical);
	countdownForm->addRow(blink);
	countdownForm->addRow(elapsed);

	auto *colorGroup = new QGroupBox(QTStr("Settings.SceneColors"));
	auto *colorForm = new QFormLayout(colorGroup);
	colorForm->addRow(QTStr("Settings.Style"), style);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	auto *layout = new QVBoxLayout(&dialog);
	layout->addWidget(countdownGroup);
	layout->addWidget(colorGroup);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted)
		return;

	settings.countdownEnabled = countdownEnabled->isChecked();
	settings.warnSeconds = warn->value();
	settings.criticalSeconds = critical->value();
	settings.blinkCritical = blink->isChecked();
	settings.showElapsed = elapsed->isChecked();
	settings.colorStyle = (SceneColorStyle)style->currentData().toInt();
	SaveSettings();

	if (onApply)
		onApply();
}
