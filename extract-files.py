#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/sony/poplardcm',
    'device/sony/yoshino-common',
    'hardware/qcom-caf/msm8998',
    'vendor/sony/yoshino-common',
]

blob_fixups: blob_fixups_user_type = {
    ('vendor/bin/hw/fpc_fingerprint@2.1_HIDL-service', 'vendor/lib64/lib_fpc_tac_shared.so'): blob_fixup()
        .replace_needed('libprotobuf-c.so', 'libprotobuf-c-idd.so'),
    'vendor/bin/hw/android.hardware.nfc@1.2-service-cxd22xx': blob_fixup()
        .add_needed('libnfc_hal_shim.so'),
    'vendor/usr/idc/clearpad.idc': blob_fixup()
        .regex_replace('/system/somc', '/vendor/etc'),
}  # fmt: skip

module = ExtractUtilsModule(
    'poplardcm',
    'sony',
    blob_fixups=blob_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    utils = ExtractUtils.device_with_common(
        module, 'yoshino-common', module.vendor
    )
    utils.run()
