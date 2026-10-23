#!/usr/bin/env bash
# Fetches the headers needed to build against the installed OBS.app
# (no Xcode needed), all into .deps/ inside the project:
#   - OBS Studio sources at a pinned tag, only libobs/ and frontend/api/
#   - Qt6 from obs-deps, the same build OBS ships (headers are used)
#   - SIMDe headers, required by the libobs headers
# Bump OBS_VERSION / OBS_DEPS_VERSION (and the hashes) when OBS updates;
# OBS's CMakePresets.json lists the obs-deps version for each release.
set -euo pipefail

cd "$(dirname "$0")/.."

OBS_VERSION="32.2.2"
OBS_DEPS_VERSION="2026-07-15"
SIMDE_VERSION="0.8.2"
SIMDE_SHA256="59068edc3420e75c5ff85ecfd80a77196fb3a151227a666cc20abb313a5360bf"

case "$(uname -m)" in
arm64)
	QT_ARCH="arm64"
	QT_SHA256="184538177d854664c56e8524e1afb2b6156083579426f3193e90c6cf4d514583"
	;;
x86_64)
	QT_ARCH="x86_64"
	QT_SHA256="5e137db187f9acb420d7ea079b4fe60e0a0d23e9e1c80a2b9597130a7e6923a5"
	;;
*)
	echo "Unsupported architecture: $(uname -m)" >&2
	exit 1
	;;
esac

DEPS=".deps"
mkdir -p "$DEPS"

installed=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
	/Applications/OBS.app/Contents/Info.plist 2>/dev/null || true)
if [[ -n "$installed" && "$installed" != "$OBS_VERSION" ]]; then
	echo "Note: installed OBS is $installed, headers are pinned to $OBS_VERSION." >&2
fi

verify() { # <file> <sha256>
	local actual
	actual=$(shasum -a 256 "$1" | awk '{print $1}')
	if [[ "$actual" != "$2" ]]; then
		echo "Checksum mismatch for $1" >&2
		echo "  expected $2" >&2
		echo "  got      $actual" >&2
		rm -f "$1"
		exit 1
	fi
}

if [[ ! -f "$DEPS/obs-studio/libobs/obs.h" ]]; then
	echo "==> OBS Studio $OBS_VERSION headers"
	rm -rf "$DEPS/obs-studio"
	git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$OBS_VERSION" \
		--filter=blob:none --sparse https://github.com/obsproject/obs-studio.git "$DEPS/obs-studio"
	git -C "$DEPS/obs-studio" sparse-checkout set libobs frontend/api
fi

if [[ ! -d "$DEPS/qt6/lib/QtWidgets.framework" ]]; then
	echo "==> Qt6 from obs-deps $OBS_DEPS_VERSION ($QT_ARCH)"
	archive="$DEPS/macos-deps-qt6-$OBS_DEPS_VERSION-$QT_ARCH.tar.xz"
	curl -fL --progress-bar -o "$archive" \
		"https://github.com/obsproject/obs-deps/releases/download/$OBS_DEPS_VERSION/macos-deps-qt6-$OBS_DEPS_VERSION-$QT_ARCH.tar.xz"
	verify "$archive" "$QT_SHA256"
	rm -rf "$DEPS/qt6"
	mkdir -p "$DEPS/qt6"
	tar -xJf "$archive" -C "$DEPS/qt6"
fi

if [[ ! -f "$DEPS/simde/simde/x86/sse2.h" ]]; then
	echo "==> SIMDe $SIMDE_VERSION"
	archive="$DEPS/simde-amalgamated-$SIMDE_VERSION.tar.xz"
	curl -fL --progress-bar -o "$archive" \
		"https://github.com/simd-everywhere/simde/releases/download/v$SIMDE_VERSION/simde-amalgamated-$SIMDE_VERSION.tar.xz"
	verify "$archive" "$SIMDE_SHA256"
	rm -rf "$DEPS/simde"
	mkdir -p "$DEPS/simde"
	tar -xJf "$archive" -C "$DEPS/simde"
	mv "$DEPS/simde/simde-amalgamated-$SIMDE_VERSION" "$DEPS/simde/simde"
fi

echo "Dependencies ready in $DEPS/"
