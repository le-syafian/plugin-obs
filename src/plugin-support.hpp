#pragma once

#include <obs-module.h>

#include <QString>

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "scene-color-countdown"
#endif

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

#define plog(level, format, ...) blog(level, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

/* Localized string from the plugin's data/locale files */
inline QString QTStr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}
