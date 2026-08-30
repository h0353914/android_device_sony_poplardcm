#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/sony/poplardcm

# Partitions
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 5242880000
BOARD_VENDORIMAGE_PARTITION_SIZE := 1610612736
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4

# Inherit common board configs
include device/sony/yoshino-common/BoardConfigCommon.mk

# Display
TARGET_SCREEN_DENSITY := 430

# Kernel
TARGET_KERNEL_CONFIG += sony/poplar_dcm.config

# Properties
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/init/fstab.qcom

# Inherit vendor board configs
include vendor/sony/poplardcm/BoardConfigVendor.mk
