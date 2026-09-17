#
# Copyright (C) 2021-2023 KonstaKANG
#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/brcm/rpi5

# Inherit device configuration
$(call inherit-product, $(DEVICE_PATH)/device.mk)

PRODUCT_AAPT_CONFIG := normal mdpi hdpi
PRODUCT_AAPT_PREF_CONFIG := hdpi
PRODUCT_CHARACTERISTICS := automotive,nosdcard

$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, packages/services/Car/car_product/build/car.mk)
$(call enforce-product-packages-exist,Bluetooth Keyguard Launcher2 OverviewApp RotaryIME RotaryPlayground com.android.ranging display_compat_config libnfc_ndef libvariablespeed pppd)

# android.car
PRODUCT_PACKAGES += \
    liblargeparcelablejni

# Audio
PRODUCT_PACKAGES += \
    android.hardware.automotive.audiocontrol-service.example

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/car/car_audio_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/car_audio_configuration.xml

# Bluetooth
PRODUCT_VENDOR_PROPERTIES += \
    bluetooth.device.class_of_device=38,4,8 \
    bluetooth.profile.asha.central.enabled=false \
    bluetooth.profile.bap.broadcast.assist.enabled=false \
    bluetooth.profile.bap.unicast.client.enabled=false \
    bluetooth.profile.bas.client.enabled=false \
    bluetooth.profile.ccp.server.enabled=false \
    bluetooth.profile.csip.set_coordinator.enabled=false \
    bluetooth.profile.hap.client.enabled=false \
    bluetooth.profile.hfp.ag.enabled=false \
    bluetooth.profile.hid.device.enabled=false \
    bluetooth.profile.hid.host.enabled=false \
    bluetooth.profile.map.server.enabled=false \
    bluetooth.profile.mcp.server.enabled=false \
    bluetooth.profile.opp.enabled=false \
    bluetooth.profile.pbap.server.enabled=false \
    bluetooth.profile.sap.server.enabled=false \
    bluetooth.profile.vcp.controller.enabled=false

# Broadcast radio
PRODUCT_PACKAGES += \
    android.hardware.broadcastradio-service.default

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.broadcastradio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.broadcastradio.xml

# Camera
ENABLE_CAMERA_SERVICE := true

# CAN
PRODUCT_PACKAGES += \
    android.hardware.automotive.can-service

PRODUCT_PACKAGES += \
    canhalctrl \
    canhaldump \
    canhalsend

# Display
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/car/display_settings.xml:$(TARGET_COPY_OUT_VENDOR)/etc/display_settings.xml

# EVS
PRODUCT_PACKAGES += \
    android.hardware.automotive.evs-v4l2 \
    cardisplayproxyd \
    CarEvsCameraPreviewApp \
    evs_app \
    evsmanagerd

PRODUCT_COPY_FILES += \
    packages/services/Car/cpp/evs/manager/aidl/init.evs.rc:$(TARGET_COPY_OUT_SYSTEM)/etc/init/init.evs.rc

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/car/evs_config_override.json:${TARGET_COPY_OUT_VENDOR}/etc/automotive/evs/config_override.json

include packages/services/Car/cpp/evs/apps/sepolicy/evsapp.mk

# Keylayout
PRODUCT_PACKAGES += \
    generic_keylayout_rpi

$(call soong_config_set_bool,rpi_keylayout,use_generic_keylayout_rpi,true)

# Occupant awareness
PRODUCT_PACKAGES += \
    android.hardware.automotive.occupant_awareness@1.0-service

include packages/services/Car/car_product/occupant_awareness/OccupantAwareness.mk

# Overlays
PRODUCT_PACKAGES += \
    AndroidRpiOverlay \
    BluetoothRpiOverlay \
    CarServiceRpiOverlay \
    SettingsProviderRpiOverlay \
    WifiRpiOverlay

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.activities_on_secondary_displays.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.activities_on_secondary_displays.xml \
    frameworks/native/data/etc/car_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/car_core_hardware.xml

# Vehicle
# ARDA_RPI5_VHAL_PORT_V1
PRODUCT_PACKAGES += \
    arda-vhal-base-service

# ARDA_RPI5_MAPS_AND_INPUT_PORT_V1

# Google Maps/GMS Chimera mount namespace and package visibility fixes.
PRODUCT_PACKAGES += \
    ArdaFrameworkGoogleVisibilityOverlay

PRODUCT_COPY_FILES += \
    vendor/arda/configs/arda-google-sysconfig.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/sysconfig/arda-google-sysconfig.xml

# Platform-signed persistent input bridge for ARDA AAP input injection.
PRODUCT_PACKAGES += ArdaInputBridge
PRODUCT_PACKAGES_DEBUG += arda-input-bridge-probe

# Device identifier. This must come after all inclusions.
PRODUCT_DEVICE := rpi5
PRODUCT_NAME := aosp_rpi5_car
PRODUCT_BRAND := Raspberry
PRODUCT_MODEL := Pi 5
PRODUCT_MANUFACTURER := Raspberry

# ARDA_RPI5_CORE_PRODUCT_PORT_V1

# ARDA Vehicle Manager and reference smoke adapter.
$(call inherit-product-if-exists, vendor/arda/vehicle/arda_vehicle.mk)
$(call inherit-product-if-exists, vendor/arda/vehicle/arda_vehicle_smoke.mk)

# Google Automotive / GAS integration.
$(call inherit-product-if-exists, vendor/arda/gas_car/arda_gas_car.mk)
$(call inherit-product-if-exists, vendor/arda/gas_car/arda_google_core_product.mk)

# Google Maps AAOS VMS early-start overlay.
PRODUCT_PACKAGES += ArdaCarServiceVmsOverlay

# Exact signed split APK bytes captured from the known-good ARDA runtime.
PRODUCT_PACKAGES += \
    ArdaMapsSplitArm64 \
    ArdaMapsSplitMdpi \
    ArdaGmsSplitAllLocales \
    ArdaGmsSplitEn \
    ArdaGmsSplitMdpi \
    ArdaVendingSplitArm64 \
    ArdaVendingSplitEn

# ARDA GNSS AIDL HAL.
$(call inherit-product-if-exists, vendor/arda/gnss/arda_gnss.mk)

# ARDA connectivity transport core: ArdaPlatformPriv + arda-connectivityd.
$(call inherit-product-if-exists, vendor/arda/ardaplatform/arda_connectivity_product.mk)
