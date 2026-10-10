#!/usr/bin/env bash
# Reproducible ARMHF PortMaster cross-build from the AAL/PortMaster SDK.
# Usage: ./scripts/build_h700_armhf.sh /path/to/AAL-ARMHF-buildkit-clang17-portmaster
set -euo pipefail
SDK="$(cd "${1:?Pass the extracted ARMHF buildkit directory}" && pwd)"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-h700-armhf"
LINK="$BUILD/link-only"
mkdir -p "$LINK"
command -v clang++ >/dev/null || { echo "Need Clang 17" >&2; exit 2; }
command -v python3 >/dev/null || { echo "Need Python3" >&2; exit 2; }
command -v ninja >/dev/null || { echo "Need Ninja" >&2; exit 2; }
# Clang cross-compiles using the sysroot; choose the installed host libstdc++
# headers for C++17 syntax. ARMHF libstdc++ is linked from the SDK itself.
CXX_VERSION="${AIRXONIX_HOST_CXX_VERSION:-$(basename "$(find /usr/include/c++ -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)")}" 
CXX_HEADERS="/usr/include/c++/$CXX_VERSION"
HOST_GNU_TRIPLE="$(gcc -dumpmachine)"
CXX_TARGET_HEADERS="/usr/include/$HOST_GNU_TRIPLE/c++/$CXX_VERSION"
[ -d "$CXX_HEADERS" ] && [ -d "$CXX_TARGET_HEADERS" ] || { echo "Need host libstdc++ headers: $CXX_HEADERS and $CXX_TARGET_HEADERS" >&2; exit 2; }
LIB="$SDK/portmaster/armhf/lib"
[ -f "$SDK/sysroot/lib/ld-linux-armhf.so.3" ] || { echo "SDK ARMHF sysroot missing" >&2; exit 2; }
ln -sfn "$LIB/libstdc++.so.6.0.32" "$LINK/libstdc++.so"
ln -sfn "$LIB/libgcc_s.so.1" "$LINK/libgcc_s.so"
ln -sfn "$LIB/libSDL2-2.0.so.0.2800.5" "$LINK/libSDL2.so"
ln -sfn "$LIB/libSDL2-2.0.so.0.2800.5" "$LINK/libSDL2-2.0.so.0"
# Firmware OpenGL ES link-only shim. Never install this shared object.
python3 - "$ROOT/src/platform/gles2_compat.hpp" "$LINK/gles_stubs.c" <<'PY'
from pathlib import Path
import re,sys
source=Path(sys.argv[1]).read_text()
names=sorted(set(re.findall(r'\b(gl[A-Z]\w*)\s*\(',source))|{'glReadPixels'})
Path(sys.argv[2]).write_text('/* Link-time only; never distribute. */\n'+''.join('void %s(void){}\n'%n for n in names))
PY
clang --target=arm-linux-gnueabihf --sysroot="$SDK/sysroot" \
    -march=armv7-a -mfpu=neon -mfloat-abi=hard -fPIC -shared -nostdlib \
    -fuse-ld=lld -Wl,-soname,libGLESv2.so.2 "$LINK/gles_stubs.c" \
    -o "$LINK/libGLESv2.so"
cat > "$BUILD/h700-armhf-toolchain.cmake" <<EOT
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET arm-linux-gnueabihf)
set(CMAKE_SYSROOT "$SDK/sysroot")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon -mfloat-abi=hard -nostdinc++ -include $ROOT/cmake/armhf_old_glibc_compat.hpp -isystem $CXX_HEADERS -isystem $CXX_TARGET_HEADERS -isystem $CXX_HEADERS/backward -B$SDK/gcc-support")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld -L$LINK -L$SDK/gcc-support -Wl,--allow-shlib-undefined -Wl,-rpath-link,$LIB")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
EOT
cmake -S "$ROOT" -B "$BUILD/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$BUILD/h700-armhf-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=TRUE \
    -DAIRXONIX_BUILD_TESTS=OFF -DAIRXONIX_BUILD_RUNTIME=ON \
    -DAIRXONIX_RESOURCE_FREE=ON \
    -DAIRXONIX_ARMHF_SDK_ROOT="$SDK" -DAIRXONIX_ARMHF_LINK_DIR="$LINK"
cmake --build "$BUILD/build" --target airxonix -j "$(nproc)"
file "$BUILD/build/airxonix"
echo "Built $BUILD/build/airxonix (rename to airxonix_mp for PortMaster)"
echo "Remember: do NOT copy the generated GLES2 link-only stub to your console."
