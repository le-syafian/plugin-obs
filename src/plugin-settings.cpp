#include "plugin-settings.hpp"
#include "plugin-support.hpp"
#include "ui-helpers.hpp"

#include <util/platform.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <memory>

namespace {

PluginSettings settings;

constexpr const char *kSettingsFile = "settings.json";

QColor ReadColor(obs_data_t *data, const char *name, const QColor &fallback)
{
	const QColor color = QColor::fromString(QLatin1StringView(obs_data_get_string(data, name)));
	return color.isValid() ? color : fallback;
}

void WriteColor(obs_data_t *data, const char *name, const QColor &color)
{
	obs_data_set_string(data, name, color.name(QColor::HexRgb).toUtf8().constData());
}

/* Button showing a color; clicking it opens a color picker */
QPushButton *ColorButton(const std::shared_ptr<QColor> &color)
{
	auto *button = new QPushButton;
	auto refresh = [button, color]() {
		button->setIcon(ColorSwatchIcon(*color));
		button->setText(color->name(QColor::HexRgb).toUpper());
	};
	refresh();

	QObject::connect(button, &QPushButton::clicked, button, [button, color, refresh]() {
		const QColor picked = QColorDialog::getColor(*color, button->window(), QString(),
							     QColorDialog::DontUseNativeDialog);
		if (picked.isValid()) {
			*color = picked;
			refresh();
		}
	});
	return button;
}

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
	obs_data_set_default_bool(data, "preview_controls_enabled", defaults.previewControlsEnabled);
	obs_data_set_default_bool(data, "tally_enabled", defaults.tallyEnabled);
	obs_data_set_default_int(data, "tally_style", (int)defaults.tallyStyle);
	obs_data_set_default_int(data, "color_style", (int)defaults.colorStyle);

	settings.countdownEnabled = obs_data_get_bool(data, "countdown_enabled");
	settings.warnSeconds = (int)obs_data_get_int(data, "warn_seconds");
	settings.criticalSeconds = (int)obs_data_get_int(data, "critical_seconds");
	settings.blinkCritical = obs_data_get_bool(data, "blink_critical");
	settings.showElapsed = obs_data_get_bool(data, "show_elapsed");
	settings.previewControlsEnabled = obs_data_get_bool(data, "preview_controls_enabled");
	settings.tallyEnabled = obs_data_get_bool(data, "tally_enabled");
	settings.tallyProgramColor = ReadColor(data, "tally_program_color", defaults.tallyProgramColor);
	settings.tallyPreviewColor = ReadColor(data, "tally_preview_color", defaults.tallyPreviewColor);
	settings.tallyStyle = obs_data_get_int(data, "tally_style") == (int)TallyStyle::Fill ? TallyStyle::Fill
											     : TallyStyle::Frame;
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
	obs_data_set_bool(data, "preview_controls_enabled", settings.previewControlsEnabled);
	obs_data_set_bool(data, "tally_enabled", settings.tallyEnabled);
	WriteColor(data, "tally_program_color", settings.tallyProgramColor);
	WriteColor(data, "tally_preview_color", settings.tallyPreviewColor);
	obs_data_set_int(data, "tally_style", (int)settings.tallyStyle);
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

	/* Program countdown */
	auto *countdownEnabled = new QCheckBox(QTStr("Settings.ShowCountdown"));
	auto *warn = new QSpinBox;
	auto *critical = new QSpinBox;
	auto *blink = new QCheckBox(QTStr("Settings.Blink"));
	auto *elapsed = new QCheckBox(QTStr("Settings.ShowElapsed"));

	for (QSpinBox *spin : {warn, critical}) {
		spin->setRange(1, 600);
		spin->setSuffix(QTStr("Settings.SecondsSuffix"));
	}

	countdownEnabled->setChecked(settings.countdownEnabled);
	warn->setValue(settings.warnSeconds);
	critical->setValue(settings.criticalSeconds);
	blink->setChecked(settings.blinkCritical);
	elapsed->setChecked(settings.showElapsed);

	/* critical threshold can never be above the warning threshold */
	QObject::connect(warn, &QSpinBox::valueChanged, critical, [critical](int value) {
		if (critical->value() > value)
			critical->setValue(value);
	});
	QObject::connect(critical, &QSpinBox::valueChanged, warn, [warn](int value) {
		if (warn->value() < value)
			warn->setValue(value);
	});

	auto syncCountdown = [countdownEnabled, warn, critical, blink]() {
		const bool on = countdownEnabled->isChecked();
		for (QWidget *widget : std::initializer_list<QWidget *>{warn, critical, blink})
			widget->setEnabled(on);
	};
	QObject::connect(countdownEnabled, &QCheckBox::toggled, &dialog, syncCountdown);
	syncCountdown();

	auto *countdownGroup = new QGroupBox(QTStr("Settings.Countdown"));
	auto *countdownForm = new QFormLayout(countdownGroup);
	countdownForm->addRow(countdownEnabled);
	countdownForm->addRow(QTStr("Settings.WarnSeconds"), warn);
	countdownForm->addRow(QTStr("Settings.CriticalSeconds"), critical);
	countdownForm->addRow(blink);
	countdownForm->addRow(elapsed);

	/* Preview media controls */
	auto *previewControls = new QCheckBox(QTStr("Settings.ShowPreviewControls"));
	previewControls->setChecked(settings.previewControlsEnabled);

	auto *previewGroup = new QGroupBox(QTStr("Settings.PreviewGroup"));
	auto *previewForm = new QFormLayout(previewGroup);
	previewForm->addRow(previewControls);

	/* Tally */
	auto *tallyEnabled = new QCheckBox(QTStr("Settings.ShowTally"));
	auto programColor = std::make_shared<QColor>(settings.tallyProgramColor);
	auto previewColor = std::make_shared<QColor>(settings.tallyPreviewColor);
	QPushButton *programButton = ColorButton(programColor);
	QPushButton *previewButton = ColorButton(previewColor);
	auto *tallyStyle = new QComboBox;
	tallyStyle->addItem(QTStr("Settings.TallyStyle.Frame"), (int)TallyStyle::Frame);
	tallyStyle->addItem(QTStr("Settings.TallyStyle.Fill"), (int)TallyStyle::Fill);
	tallyEnabled->setChecked(settings.tallyEnabled);
	tallyStyle->setCurrentIndex(tallyStyle->findData((int)settings.tallyStyle));

	auto syncTally = [tallyEnabled, programButton, previewButton, tallyStyle]() {
		const bool on = tallyEnabled->isChecked();
		for (QWidget *widget : std::initializer_list<QWidget *>{programButton, previewButton, tallyStyle})
			widget->setEnabled(on);
	};
	QObject::connect(tallyEnabled, &QCheckBox::toggled, &dialog, syncTally);
	syncTally();

	auto *tallyGroup = new QGroupBox(QTStr("Settings.TallyGroup"));
	auto *tallyForm = new QFormLayout(tallyGroup);
	tallyForm->addRow(tallyEnabled);
	tallyForm->addRow(QTStr("Settings.ProgramColor"), programButton);
	tallyForm->addRow(QTStr("Settings.PreviewColor"), previewButton);
	tallyForm->addRow(QTStr("Settings.TallyStyle"), tallyStyle);

	/* Scene colors */
	auto *style = new QComboBox;
	style->addItem(QTStr("Settings.Style.Fill"), (int)SceneColorStyle::Fill);
	style->addItem(QTStr("Settings.Style.Stripe"), (int)SceneColorStyle::Stripe);
	style->setCurrentIndex(style->findData((int)settings.colorStyle));

	auto *colorGroup = new QGroupBox(QTStr("Settings.SceneColors"));
	auto *colorForm = new QFormLayout(colorGroup);
	colorForm->addRow(QTStr("Settings.Style"), style);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	auto *layout = new QVBoxLayout(&dialog);
	layout->addWidget(countdownGroup);
	layout->addWidget(previewGroup);
	layout->addWidget(tallyGroup);
	layout->addWidget(colorGroup);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted)
		return;

	settings.countdownEnabled = countdownEnabled->isChecked();
	settings.warnSeconds = warn->value();
	settings.criticalSeconds = critical->value();
	settings.blinkCritical = blink->isChecked();
	settings.showElapsed = elapsed->isChecked();
	settings.previewControlsEnabled = previewControls->isChecked();
	settings.tallyEnabled = tallyEnabled->isChecked();
	settings.tallyProgramColor = *programColor;
	settings.tallyPreviewColor = *previewColor;
	settings.tallyStyle = (TallyStyle)tallyStyle->currentData().toInt();
	settings.colorStyle = (SceneColorStyle)style->currentData().toInt();
	SaveSettings();

	if (onApply)
		onApply();
}
