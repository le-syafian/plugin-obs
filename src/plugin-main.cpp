#include "media-countdown.hpp"
#include "plugin-settings.hpp"
#include "plugin-support.hpp"
#include "scene-colors.hpp"

#include <obs-frontend-api.h>

#include <QAction>
#include <QMainWindow>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Scene Color & Program Countdown";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Plugin.Description");
}

namespace {

QPointer<SceneColorManager> sceneColors;
QPointer<MediaCountdownController> countdown;
QPointer<QAction> settingsAction;

void OpenSettings()
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	ShowSettingsDialog(main, []() {
		if (sceneColors)
			sceneColors->ApplySettings();
		if (countdown)
			countdown->UpdatePlacement();
	});
}

void Attach()
{
	if (sceneColors || countdown)
		return;

	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		plog(LOG_WARNING, "Main window not available");
		return;
	}

	sceneColors = new SceneColorManager(main);
	countdown = new MediaCountdownController(main);

	settingsAction = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("Settings.Menu")));
	if (settingsAction)
		QObject::connect(settingsAction.data(), &QAction::triggered, countdown.data(), OpenSettings);
}

/* Everything we created must be gone before OBS unloads the module, or Qt
 * would later run destructors from unloaded code. */
void Detach()
{
	delete settingsAction.data();
	delete countdown.data();
	delete sceneColors.data();
}

void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		Attach();
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		if (sceneColors)
			sceneColors->ScheduleRefresh();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		if (sceneColors)
			sceneColors->RefreshTally();
		if (countdown)
			countdown->Tick();
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		if (countdown)
			countdown->UpdatePlacement();
		if (sceneColors)
			sceneColors->RefreshTally();
		break;
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		if (sceneColors)
			sceneColors->Repaint();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		Detach();
		break;
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	LoadSettings();
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
	plog(LOG_INFO, "Loaded version %s", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	Detach();
}
