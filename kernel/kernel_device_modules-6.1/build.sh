# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

# DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

build_scope=internal
if [ ! -d "vendor/mediatek/tests/kernel" ]
then
  build_scope=customer
fi

# Default to kernel-only build (skip modules and DLKM images)
# To build modules instead, set TARGET explicitly before running
if [ -z ${TARGET} ]
then
  TARGET=${PROJECT}_kernel_aarch64
  KLEAF_BUILD_TARGET=//${DEVICE_MODULES_DIR}:${TARGET}.${MODE}
  SKIP_DIST=1
else
  KLEAF_BUILD_TARGET=${TARGET}.${MODE}
fi
KLEAF_DIST_TARGET=//${DEVICE_MODULES_DIR}:${PROJECT}_${build_scope}_dist.${MODE}

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} \
	--config=stamp --repo_manifest=${ROOT_DIR}/${DEVICE_MODULES_DIR}/fake_manifest.xml \
	--experimental_writable_outputs \
	--//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION_NUM}")

set -x
(
  export ${BAZEL_EXPORT_ENV} && \
	tools/bazel ${KLEAF_OUT} build ${KLEAF_ARGS} ${KLEAF_BUILD_TARGET}
  # Skip dist target for kernel-only build (dist bundles kernel + modules)
  if [ -z ${SKIP_DIST} ]; then
    export ${BAZEL_EXPORT_ENV} && \
	  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} \
	  --nokmi_symbol_list_violations_check ${KLEAF_DIST_TARGET} -- --dist_dir=${OUT_DIR}/dist
  fi
)
set +x

