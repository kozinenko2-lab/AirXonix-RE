#!/usr/bin/env bash
# AirXonix RE — native AArch64 / Cortex-A53 / H700 cross-build.
# Requires Clang 17 + LLD, Ninja, CMake and extracted PortMaster-H700-SYSROOT.
# No original commercial game code or third-party game port required.
set -euo pipefail
if [ "$#" -ne 1 ]; then echo "Usage: $0 /path/to/PortMaster-H700-SYSROOT" >&2; exit 2; fi
SDK="$(cd "$1" && pwd)"
SOURCE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$SOURCE/build-h700-arm64"
LINK="$OUT/link-only"
SYS="$SDK/sysroot"
mkdir -p "$OUT" "$LINK" "$SYS/usr/aarch64-linux-gnu/lib"
for tool in clang clang++ cmake ninja python3 readelf ar; do command -v "$tool" >/dev/null || exit 2; done
for f in libc.so.6 libc_nonshared.a ld-linux-aarch64.so.1 libpthread.so.0 libpthread_nonshared.a; do
  ln -sfn "../../lib/$f" "$SYS/usr/aarch64-linux-gnu/lib/$f"
done
ar rcs "$LINK/libgcc.a"
ln -sfn "$SYS/usr/lib/libgcc_s.so.1" "$LINK/libgcc_s.so"
ln -sfn "$SDK/runtime-libs/aarch64/lib/libSDL2-2.0.so.0.2800.5" "$LINK/libSDL2.so"
# The GLES2 stub is ONLY used to link. Never distribute it.
python3 - "$SOURCE/src/platform/gles2_compat.hpp" "$LINK/gles_link_only.c" <<'PY'
import re,sys
from pathlib import Path
src=Path(sys.argv[1]).read_text()
names=sorted(set(re.findall(r'\b(gl[A-Z]\w*)\s*\(',src)) | {'glReadPixels'})
Path(sys.argv[2]).write_text('/* LINK ONLY; NEVER DISTRIBUTE */\n'+''.join('void %s(void){}\n'%x for x in names))
PY
clang --target=aarch64-linux-gnu --sysroot="$SYS" -march=armv8-a -mcpu=cortex-a53 -shared -nostdlib -fuse-ld=lld -Wl,-soname,libGLESv2.so.2 "$LINK/gles_link_only.c" -o "$LINK/libGLESv2.so"
cat > "$LINK/csu.c" <<'C'
typedef void (*ax_init_t)(int,char**,char**);
extern ax_init_t __init_array_start[], __init_array_end[];
extern void _init(void),_fini(void);
void *__dso_handle=&__dso_handle;
void __libc_csu_init(int argc,char** argv,char** envp){
 _init();for(ax_init_t* p=__init_array_start;p!=__init_array_end;++p)(*p)(argc,argv,envp);
}
void __libc_csu_fini(void){_fini();}
C
clang --target=aarch64-linux-gnu --sysroot="$SYS" -march=armv8-a -c "$LINK/csu.c" -o "$LINK/csu.o"
V="$(find /usr/include/c++ -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)"
HEADER="$V"
HEADER_ARCH="/usr/include/$(gcc -dumpmachine)/c++/$(basename "$V")"
cat > "$OUT/aarch64-clang-portmaster.cmake" <<EOC
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_SYSROOT "$SYS")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a -mcpu=cortex-a53 -nostdinc++ -include $SOURCE/cmake/arm64_old_glibc_compat.hpp -isystem $HEADER -isystem $HEADER_ARCH -isystem $HEADER/backward")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -no-pie -fuse-ld=lld -Wl,-dynamic-linker,/lib/ld-linux-aarch64.so.1 -Wl,--allow-shlib-undefined -L$LINK -L$SDK/runtime-libs/aarch64/lib -L$SYS/usr/lib -Wl,-rpath-link,$SDK/runtime-libs/aarch64/lib $SYS/usr/lib/crt1.o $SYS/usr/lib/crti.o $LINK/csu.o")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
EOC
cmake -S "$SOURCE" -B "$OUT/build" -G Ninja \
 -DCMAKE_TOOLCHAIN_FILE="$OUT/aarch64-clang-portmaster.cmake" \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=TRUE \
 -DAIRXONIX_BUILD_RUNTIME=ON -DAIRXONIX_BUILD_TESTS=OFF \
 -DAIRXONIX_RESOURCE_FREE=ON -DAIRXONIX_PORTMASTER_ARM64_COMPAT=ON \
 -DAIRXONIX_ARM64_SDK_ROOT="$SDK" -DAIRXONIX_ARM64_LINK_DIR="$LINK"
cmake --build "$OUT/build" --target airxonix -j4
readelf -h "$OUT/build/airxonix" | grep 'AArch64'
readelf -d "$OUT/build/airxonix" | grep 'libGLESv2.so.2'
echo "ARM64 build: $OUT/build/airxonix"
