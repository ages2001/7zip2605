#!/usr/bin/env bash
# Rebuild every runtime archive used by the port with an i386 CPU baseline.
set -euo pipefail
nt9x_tools=${NT9X_TOOLS:-/workspace/nt9x-tools}
nt9x_jobs=${NT9X_JOBS:-4}
export PATH="$nt9x_tools/root/usr/bin:$PATH"
nt9x_cc=i686-w64-mingw32-gcc-win32
nt9x_cxx=i686-w64-mingw32-g++-win32
nt9x_gcc="$nt9x_tools/gcc-source/gcc-14.2.0"
mkdir -p "$nt9x_tools/i386-crt" "$nt9x_tools/gcc-config" "$nt9x_tools/build-x86_64-pc-linux-gnu/libiberty" "$nt9x_tools/i386-libgcc" "$nt9x_tools/i386-libstdcxx"
(cd "$nt9x_tools/i386-crt"
 if [[ ! -f Makefile ]]; then
  "$nt9x_tools/mingw-src/mingw-w64-crt/configure" --host=i686-w64-mingw32 --prefix="$nt9x_tools/i386-crt-install" --with-sysroot="$nt9x_tools/root/usr/i686-w64-mingw32" --enable-lib32 --disable-lib64 --with-default-msvcrt=crtdll CC="$nt9x_cc" CXX="$nt9x_cxx" CFLAGS='-O2 -march=i386' CXXFLAGS='-O2 -march=i386'
 fi
 mkdir -p lib32
 make -j"$nt9x_jobs" lib32/crt2.o lib32/dllcrt2.o lib32/libmingw32.a lib32/libmingwex.a lib32/libcrtdll.a lib32/libmoldname.a)
(cd "$nt9x_tools/build-x86_64-pc-linux-gnu/libiberty"
 if [[ ! -f Makefile ]]; then "$nt9x_gcc/libiberty/configure" --disable-nls;fi
 make -j"$nt9x_jobs")
(cd "$nt9x_tools/gcc-config"
 if [[ ! -f Makefile ]]; then
  "$nt9x_gcc/gcc/configure" --target=i686-w64-mingw32 --prefix="$nt9x_tools/i386-gcc" --disable-bootstrap --disable-multilib --disable-nls --enable-languages=c,c++ --enable-threads=win32 --with-arch=i386 --disable-sjlj-exceptions --with-dwarf2 --with-sysroot="$nt9x_tools/root/usr" --with-mpfr-include="$nt9x_tools/root/usr/include" --with-mpfr-lib="$nt9x_tools/root/usr/lib/x86_64-linux-gnu" --with-mpc-include="$nt9x_tools/root/usr/include" --with-mpc-lib="$nt9x_tools/root/usr/lib/x86_64-linux-gnu"
 fi
 # Use the already verified cross compiler; only GCC-generated headers are needed.
 make -j"$nt9x_jobs" -o specs -o xgcc libgcc.mvars tconfig.h tm.h tm_p.h options.h insn-constants.h insn-modes.h version.h)
(cd "$nt9x_tools/i386-libgcc"
 if [[ ! -f Makefile ]]; then
  "$nt9x_gcc/libgcc/configure" --host=i686-w64-mingw32 --build=x86_64-linux-gnu --prefix="$nt9x_tools/i386-runtime" --disable-shared --disable-multilib --enable-threads=win32 --disable-sjlj-exceptions CC="$nt9x_cc" CFLAGS='-O2 -march=i386 -mcrtdll=crtdll'
 fi
 mkdir -p "$nt9x_tools/gcc-config/include"
 make -j"$nt9x_jobs" gcc_objdir="$nt9x_tools/gcc-config")
(cd "$nt9x_tools/i386-libstdcxx"
 if [[ ! -f Makefile ]]; then
  "$nt9x_gcc/libstdc++-v3/configure" --host=i686-w64-mingw32 --build=x86_64-linux-gnu --prefix="$nt9x_tools/i386-runtime" --disable-shared --disable-multilib --disable-nls --disable-libstdcxx-pch --with-libstdcxx-zoneinfo=no CC="$nt9x_cc" CXX="$nt9x_cxx" CFLAGS='-O2 -march=i386 -mcrtdll=crtdll' CXXFLAGS='-O2 -march=i386 -mcrtdll=crtdll'
 fi
 # Only these archives are linked into the application. The complete upstream
 # library includes C++20 timezone support with newer Windows dependencies.
 make -C include -j"$nt9x_jobs"
 make -C libsupc++ -j"$nt9x_jobs"
 make -C src/c++98 -j"$nt9x_jobs"
 make -C src/c++11 -j"$nt9x_jobs")
