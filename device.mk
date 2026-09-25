#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Include from common device tree
$(call inherit-product, device/sony/yoshino-common/yoshino.mk)

# AAPT
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREBUILT_DPI := xxhdpi xhdpi hdpi
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

# Audio
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/audio/audio_platform_info.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_platform_info.xml \
    $(LOCAL_PATH)/configs/audio/mixer_paths_tasha.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths_tasha.xml

# Boot animation
TARGET_SCREEN_HEIGHT := 1920
TARGET_SCREEN_WIDTH := 1080

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(LOCAL_PATH)/overlay

# Ramdisk
PRODUCT_PACKAGES += \
    fstab.qcom \
    fstab.qcom.ramdisk \
    init.target.nfc.rc \
    init.felica_cfg.sh

# FeliCa（Osaifu-Keitai）各營運商設定：發行者識別碼、憑證、金鑰各不相同，
# 全部安裝到 /vendor/etc/felica/<variant>/，開機時依 oem 分割區的 ro.somc.customerid
# 選出對應目錄 bind mount 到 /vendor/etc/felica（見 init.target.nfc.rc、init.felica_cfg.sh）。
FELICA_CFG_VARIANTS := docomo softbank kddi
PRODUCT_COPY_FILES += $(foreach v,$(FELICA_CFG_VARIANTS),$(foreach f,common.cfg mfm.cfg mfs.cfg,\
    $(LOCAL_PATH)/configs/felica/$(v)/$(f):$(TARGET_COPY_OUT_VENDOR)/etc/felica/$(v)/$(f)))

# Soong
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH)

# Inherit vendor makefiles
$(call inherit-product, vendor/sony/poplardcm/poplardcm-vendor.mk)
