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
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-3lu.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/cis/is-cis-imx564.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
    "drivers/media/platform/exynos/camera/sensor/module_framework/flash/is-flash-s2mpb02-i2c.ko",
    "drivers/leds/leds-s2mpb02.ko",
    "drivers/gpu/drm/samsung/dpu/dp_ext_func/dp_ext_func.ko",
    "sound/soc/samsung/exynos/exynos9945_audio.ko",
    "drivers/firmware/cirrus/cs_dsp.ko",
    "sound/soc/codecs/snd-soc-wm-adsp.ko",
    "sound/soc/codecs/snd-soc-cirrus-amp.ko",
    "sound/soc/codecs/snd-soc-cs35l43-i2c.ko",
    "drivers/wlan/kernel_modules/qrtr/qrtr.ko",
    "drivers/wlan/kernel_modules/qrtr/qrtr-mhi.ko",
    "drivers/wlan/kernel_modules/mhi/host/mhi.ko",
    "drivers/wlan/kernel_modules/qmi_helpers/qmi_helpers.ko",
    "drivers/wlan/platform/cnss2/cnss2.ko",
    "drivers/wlan/platform/cnss_utils/cnss_utils.ko",
    "drivers/wlan/platform/cnss_prealloc/cnss_prealloc.ko",
    "drivers/wlan/platform/cnss_utils/wlan_firmware_service.ko",
    "drivers/wlan/platform/cnss_genl/cnss_nl.ko",
    "drivers/wlan/platform/cnss_utils/cnss_plat_ipc_qmi_svc.ko",
    "drivers/wlan/kernel_modules/ipc_logging/qcom_ipc_logging.ko",
    "drivers/bluetooth/btpower.ko",
]

MODEL_VARIANT_DLKM_MODULE_LIST = [
    # Please add to model variant vendor dlkm drivers
    "drivers/wlan/qcacld-3.0/kiwi_v2.ko",
]
