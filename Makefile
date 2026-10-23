# Scene Color & Program Countdown - OBS Studio plugin, macOS build.
#
# Builds straight against the installed OBS.app with the Command Line Tools
# (clang++ + make), no Xcode or CMake needed. Windows, Linux and universal
# macOS builds use CMake instead (CMakeLists.txt, buildspec.json).
#
#   make deps      fetch headers into .deps/ (once)
#   make           build build/scene-color-countdown.plugin
#   make install   copy the plugin into OBS's user plugin folder

PLUGIN      := scene-color-countdown
# Keep in sync with "version" in buildspec.json
VERSION     := 1.2.0
BUNDLE_ID   := com.le-syafian.scene-color-countdown

OBS_APP     ?= /Applications/OBS.app
ARCH        ?= $(shell uname -m)
MIN_MACOS   ?= 13.0
PLUGINS_DIR ?= $(HOME)/Library/Application Support/obs-studio/plugins

DEPS        := .deps
OBS_SRC     := $(DEPS)/obs-studio
QT_LIB      := $(DEPS)/qt6/lib
SIMDE_INC   := $(DEPS)/simde

BUILD       := build
OBJ_DIR     := $(BUILD)/obj
GEN_DIR     := $(BUILD)/generated
BUNDLE      := $(BUILD)/$(PLUGIN).plugin
BINARY      := $(BUNDLE)/Contents/MacOS/$(PLUGIN)

SOURCES     := $(wildcard src/*.cpp)
OBJECTS     := $(SOURCES:src/%.cpp=$(OBJ_DIR)/%.o)
LOCALES     := $(wildcard data/locale/*.ini)

OBS_FRAMEWORKS := $(OBS_APP)/Contents/Frameworks

CXX      := clang++
CXXFLAGS := -std=c++20 -arch $(ARCH) -mmacosx-version-min=$(MIN_MACOS) -O2 -g \
	-fvisibility=hidden -fvisibility-inlines-hidden \
	-Wall -Wextra -Wno-unused-parameter -MMD -MP \
	-DPLUGIN_NAME='"$(PLUGIN)"' -DPLUGIN_VERSION='"$(VERSION)"' -DQT_NO_DEBUG \
	-isystem $(GEN_DIR) -isystem $(OBS_SRC)/libobs -isystem $(OBS_SRC)/frontend/api \
	-isystem $(SIMDE_INC) -iframework $(QT_LIB) \
	-isystem $(QT_LIB)/QtCore.framework/Headers \
	-isystem $(QT_LIB)/QtGui.framework/Headers \
	-isystem $(QT_LIB)/QtWidgets.framework/Headers

# Link against the exact libraries OBS loads at runtime
LDFLAGS  := -arch $(ARCH) -mmacosx-version-min=$(MIN_MACOS) -bundle \
	-F$(OBS_FRAMEWORKS) -framework libobs -framework QtCore -framework QtGui -framework QtWidgets \
	$(OBS_FRAMEWORKS)/obs-frontend-api.dylib \
	-Wl,-rpath,@executable_path/../Frameworks

.PHONY: all deps check-deps install uninstall clean

all: $(BUNDLE)

deps:
	./scripts/setup-deps.sh

check-deps:
	@test -f $(OBS_SRC)/libobs/obs.h && test -d $(QT_LIB)/QtWidgets.framework && \
		test -f $(SIMDE_INC)/simde/x86/sse2.h || \
		{ echo "Dependencies missing, run 'make deps' first."; exit 1; }
	@test -d "$(OBS_APP)" || { echo "OBS not found at $(OBS_APP) (set OBS_APP=...)"; exit 1; }

# libobs headers include this generated file
$(GEN_DIR)/obsconfig.h:
	@mkdir -p $(@D)
	@printf '#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n' > $@

# Depends on the Makefile so that changed flags (e.g. VERSION) rebuild everything
$(OBJ_DIR)/%.o: src/%.cpp $(GEN_DIR)/obsconfig.h Makefile | check-deps
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUNDLE): $(BINARY) macos/Info.plist.in $(LOCALES)
	@mkdir -p $@/Contents/Resources/locale
	sed -e 's/@PLUGIN@/$(PLUGIN)/g' -e 's/@VERSION@/$(VERSION)/g' \
		-e 's/@BUNDLE_ID@/$(BUNDLE_ID)/g' -e 's/@MIN_MACOS@/$(MIN_MACOS)/g' \
		macos/Info.plist.in > $@/Contents/Info.plist
	cp $(LOCALES) $@/Contents/Resources/locale/
	codesign --force --sign - $@
	@touch $@
	@echo "Built $@"

install: $(BUNDLE)
	@mkdir -p "$(PLUGINS_DIR)"
	rm -rf "$(PLUGINS_DIR)/$(PLUGIN).plugin"
	cp -R $(BUNDLE) "$(PLUGINS_DIR)/"
	@echo "Installed to $(PLUGINS_DIR)/$(PLUGIN).plugin (restart OBS to load it)"

uninstall:
	rm -rf "$(PLUGINS_DIR)/$(PLUGIN).plugin"

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
