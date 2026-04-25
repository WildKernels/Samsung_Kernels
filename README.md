# Samsung_Kernels

ALL of these Samsung Kernels are from Samsung Opensource website.
They all include the toolchain specific from Samsung.
to go around git lfs, files larger than 50MB are compressed as .tar.xz

NOTE: Each branch is its own Kernel source. Kernels are set as the model number, oneui version, and bit revision, see more information below:

Example build number: S926BXXS7BYEC
the first 5 digits are the model number: S926B
the 5th digit from the end is the bootloader bit rev, for this case, it's bit rev 7.

The build number is in the build script to compile the kernel, eg: build_kernel_GKI.sh
If the kernel does NOT have a build script, it will be scripts/setlocalversion file near the bottom, just look for the BUILD_NUMBER variable.

feel free to use these kernels for personal use as they have not been modified, these are stock Samsung Kernels.

If you'd  like to see how they are built, you can look at the workflow in the Samsung_KernelSU_SUSFS repo: [https://github.com/WildKernels/Samsung_KernelSU_SUSFS](https://github.com/WildKernels/Samsung_KernelSU_SUSFS)
