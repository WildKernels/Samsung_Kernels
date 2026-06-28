load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")

def define_wonder(target, variant):
    """Define wonder module for specific target and variant"""
    kernel_build_variant = "{}_{}".format(target, variant)
    kernel_build_label = "//msm-kernel:{}".format(kernel_build_variant)
    
    ddk_module(
        name = "{}_wonder".format(kernel_build_variant),
        kernel_build = kernel_build_label,
        srcs = glob([
            "**/*.h",
        ]) + [
            "band_config.c",
            "mac80211.c",
            "mac80211_txs.c",
            "main.c",
            "nl80211_ven_cmd.c",
            "ssr.c",
            "wondertap.c",
        ],
        out = "wonder.ko",
        conditional_srcs = {
            "CONFIG_DEBUG_FS": {
                True: [
                    "debugfs.c",
                ],
            },
        },
        defconfig = "defconfig",
        kconfig = "Kconfig",
        visibility = ["//visibility:public"],
        deps = ["//msm-kernel:all_headers"],
    )

def define_wonder_for_targets():
    """Define wonder for all target variants"""
    # Sun target variants
    define_wonder("sun", "consolidate")
    define_wonder("sun", "perf")
    
    # Pineapple target variants (uncomment if needed)
    # define_wonder("pineapple", "consolidate")
    # define_wonder("pineapple", "gki")