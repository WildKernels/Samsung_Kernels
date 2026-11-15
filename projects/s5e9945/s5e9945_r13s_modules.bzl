# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

MODEL_VARIANT_MODULE_LIST = [
    # Please add to model variant vendor boot drivers
    # Chipset common driver have to add to {TARGET_SOC}_modules.bzl
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-gn3.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-3k1.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-3l6.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx825.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-ov08a10.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/flash/is-flash-aw36518.ko",
    "drivers/leds/leds-aw36518.ko",
    "drivers/gpu/drm/samsung/dpu/dp_ext_func/dp_ext_func.ko",
    "sound/soc/samsung/exynos/exynos9945_audio.ko",
    "sound/soc/codecs/aw882xx/snd-soc-smartpa-aw882xx.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox-sync-aw.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_platform_mif.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_mx.ko",
    "drivers/net/wireless/pcie_scsc/scsc_wifilogger.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_mmap.ko",
    # "drivers/misc/samsung/pcie_scsc/scsc_flash_service.ko",
    "drivers/misc/samsung/pcie_scsc/mx_client_test.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_log_collection.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_logring.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_boot_service.ko",
    "drivers/misc/samsung/slsi_bt/scsc_bt.ko",
]

MODEL_VARIANT_DLKM_MODULE_LIST = [
    # Please add to model variant vendor dlkm drivers
	"drivers/net/wireless/pcie_scsc/scsc_wlan.ko",
]
