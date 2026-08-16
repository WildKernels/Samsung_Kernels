#!/bin/bash

export PATH=$(pwd)/toolchain/clang/host/linux-x86/clang-r450784d/bin:$PATH
export PATH=$(pwd)/toolchain/build/kernel/build-tools/path/linux-x86/:$PATH
export HOSTCFLAGS="-I$(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/include -isystem /usr/include"
export HOSTLDFLAGS="-L $(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/lib64 \
    -Wl,-rpath,$(pwd)/toolchain/prebuilts/kernel-build-tools/linux-x86/lib64 \
    -fuse-ld=lld --rtlib=compiler-rt"

export DTC_FLAGS="-@"
export PLATFORM_VERSION=13
export ANDROID_MAJOR_VERSION=t
export LLVM=1
export DEPMOD=depmod
export ARCH=arm64
export TARGET_SOC=s5e8835
export BUILD_NUMBER=A356EXXU9DZE8
make s5e8835-a35xjvxx_defconfig
make -j$(nproc) Image
