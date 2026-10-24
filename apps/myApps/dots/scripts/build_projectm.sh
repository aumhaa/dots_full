#!/bin/sh
# Builds libprojectM (the MilkDrop engine) as static arm64 libraries into
# libs/projectM, and fetches the presets and textures into bin/data/projectM.
#
# Needs CMake (brew install cmake). Run from anywhere:
#     sh scripts/build_projectm.sh
#
# projectM is pinned to a master commit because projectm_opengl_render_frame_fbo
# (drawing into our own framebuffer) only exists from 4.2, which isn't released yet.
set -e

PROJECTM_SHA=349b7cc2db312d47cef6be24c6856ced6643a385
PROJECTM_SRC="${PROJECTM_SRC:-$HOME/openFrameworks/projectm}"
APP="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="$APP/libs/projectM"
DATA="$APP/bin/data/projectM"

if [ ! -d "$PROJECTM_SRC/.git" ]; then
    git init -q "$PROJECTM_SRC"
    git -C "$PROJECTM_SRC" remote add origin https://github.com/projectM-visualizer/projectm.git
fi
if [ "$(git -C "$PROJECTM_SRC" rev-parse HEAD 2>/dev/null)" != "$PROJECTM_SHA" ]; then
    git -C "$PROJECTM_SRC" fetch -q --depth 1 origin "$PROJECTM_SHA"
    git -C "$PROJECTM_SRC" checkout -q FETCH_HEAD
fi
git -C "$PROJECTM_SRC" submodule update -q --init --depth 1

cmake -S "$PROJECTM_SRC" -B "$PROJECTM_SRC/build-dots" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.5 \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_SYSTEM_PROJECTM_EVAL=OFF \
    -DENABLE_PLAYLIST=ON \
    -DENABLE_SDL_UI=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$PROJECTM_SRC/build-dots" --config Release -j "$(sysctl -n hw.ncpu)"
rm -rf "$PREFIX"
cmake --install "$PROJECTM_SRC/build-dots" --config Release
# Only the headers and static libraries are needed
rm -rf "$PREFIX/lib/cmake" "$PREFIX/lib/pkgconfig" "$PREFIX/share"

mkdir -p "$DATA"
[ -d "$DATA/presets" ] || git clone -q --depth 1 https://github.com/projectM-visualizer/presets-cream-of-the-crop.git "$DATA/presets"
[ -d "$DATA/textures" ] || git clone -q --depth 1 https://github.com/projectM-visualizer/presets-milkdrop-texture-pack.git "$DATA/textures"

echo "libprojectM $PROJECTM_SHA installed in $PREFIX"
