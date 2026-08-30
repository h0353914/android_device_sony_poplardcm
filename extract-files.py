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
    'device/sony/yoshino-common',
    'hardware/qcom-caf/msm8998',
    'vendor/sony/yoshino-common',
]

blob_fixups: blob_fixups_user_type = {
    # 原廠 55 模式配置：複製 Default 101 並加入同檔 Virtual PCC。
    ('vendor/etc/qdcm_calib_data_6.xml', 'vendor/etc/qdcm_calib_data_9.xml'): blob_fixup()
        .regex_replace(
            '(?s)\\A(?!.*<Mode\\b[^>]*Name="hal_hdr")(?=.*<Mode\\b[^>]*ModeID="101"[^>]*>(?P<default>.*?)</Mode>)(?=.*<Mode\\b[^>]*Name="hal_native"[^>]*>(?:(?!</Mode>).)*?(?P<pcc><Feature\\b[^>]*FeatureType="25"[^>]*>.*?</Feature>))(?P<prefix>.*?<Disp_Modes\\b[^>]*NumModes=")55(?P<content>".*?)(?P<suffix></Disp_Modes>.*)\\Z',
            '\\g<prefix>56\\g<content>    <Mode ModeID="200" DisplayID="0" IsDefaultMode="0" IsAppMode="0" Name="hal_hdr" NumOfFeatures="13" WhitePoint="0" EValue="255" BValue="100" RValue="100">\\g<default>    \\g<pcc>\\n        </Mode>\\n    \\g<suffix>',
        ),
    ('vendor/bin/hw/fpc_fingerprint@2.1_HIDL-service', 'vendor/lib64/lib_fpc_tac_shared.so'): blob_fixup()
        .replace_needed('libprotobuf-c.so', 'libprotobuf-c-idd.so'),
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
