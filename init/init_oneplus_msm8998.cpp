/*
   Copyright (c) 2014, The Linux Foundation. All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#define DEVINFO_FILE "/sys/project_info/project_name"
#define SENSOR_VERSION_FILE "/sys/devices/soc/soc:fingerprint_detect/sensor_version"

#include <android-base/properties.h>
#include "vendor_init.h"

using android::base::Trim;
using android::base::GetProperty;
using android::base::ReadFileToString;

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_multi(char const system_prop[], char const vendor_prop[],char const bootimage_prop[],
    char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
    property_override(bootimage_prop, value);
}

void load_dalvikvm_properties()
{
    struct sysinfo sys;

    sysinfo(&sys);
    if (sys.totalram < 7000ull * 1024 * 1024) {
        //6GB RAM
        property_override("dalvik.vm.heapstartsize", "16m");
        property_override("dalvik.vm.heaptargetutilization", "0.5");
        property_override("dalvik.vm.heapmaxfree", "32m");
        property_override("dalvik.vm.heapgrowthlimit", "256m");
        property_override("dalvik.vm.heapsize", "512m");
        property_override("dalvik.vm.heapminfree", "8m");
    } else {
        //8GB RAM
        property_override("dalvik.vm.heapstartsize", "24m");
        property_override("dalvik.vm.heaptargetutilization", "0.46");
        property_override("dalvik.vm.heapmaxfree", "48m");
        property_override("dalvik.vm.heapgrowthlimit", "256m");
        property_override("dalvik.vm.heapsize", "512m");
        property_override("dalvik.vm.heapminfree", "8m");
    }
}

void init_target_properties()
{
    std::string device;
    bool unknownDevice = true;

    if (ReadFileToString(DEVINFO_FILE, &device)) {
        LOG(INFO) << "Device info: " << device;

        if (!strncmp(device.c_str(), "16859", 5)) {
            // Oneplus 5
            property_override("ro.display.series", "OnePlus 5");
            unknownDevice = false;
        }
        else if (!strncmp(device.c_str(), "17801", 5)) {
            // Oneplus 5T
            property_override("ro.display.series", "OnePlus 5T");
            unknownDevice = false;
        }

        property_override("vendor.boot.project_name", device.c_str());
    }
    else {
        LOG(ERROR) << "Unable to read device info from " << DEVINFO_FILE;
    }

    if (unknownDevice) {
        property_override("ro.display.series", "UNKNOWN");
    }
}

void init_fingerprint_properties()
{
    std::string sensor_version;

    if (ReadFileToString(SENSOR_VERSION_FILE, &sensor_version)) {
        LOG(INFO) << "Loading Fingerprint HAL for sensor version " << sensor_version;
        if (Trim(sensor_version) == "1" || Trim(sensor_version) == "2") {
            property_override("ro.hardware.fingerprint", "fpc");
        }
        else if (Trim(sensor_version) == "3") {
            property_override("ro.hardware.fingerprint", "goodix");
        }
        else {
            LOG(ERROR) << "Unsupported fingerprint sensor: " << sensor_version;
        }
    }
    else {
        LOG(ERROR) << "Failed to detect sensor version";
    }
}

void init_alarm_boot_properties()
{
    char const *boot_reason_file = "/proc/sys/kernel/boot_reason";
    std::string boot_reason;

    if (ReadFileToString(boot_reason_file, &boot_reason)) {
        /*
         * Setup ro.vendor.alarm_boot value to true when it is RTC triggered boot up
         * For existing PMIC chips, the following mapping applies
         * for the value of boot_reason:
         *
         * 0 -> unknown
         * 1 -> hard reset
         * 2 -> sudden momentary power loss (SMPL)
         * 3 -> real time clock (RTC)
         * 4 -> DC charger inserted
         * 5 -> USB charger inserted
         * 6 -> PON1 pin toggled (for secondary PMICs)
         * 7 -> CBLPWR_N pin toggled (for external power supply)
         * 8 -> KPDPWR_N pin toggled (power key pressed)
         */
        if (Trim(boot_reason) == "0") {
            property_override("ro.boot.bootreason", "invalid");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "1") {
            property_override("ro.boot.bootreason", "hard_reset");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "2") {
            property_override("ro.boot.bootreason", "smpl");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "3") {
            property_override("ro.vendor.alarm_boot", "true");
            // disable boot animation for RTC wakeup
            property_override("debug.sf.nobootanimation", "1");
        }
        else if (Trim(boot_reason) == "4") {
            property_override("ro.boot.bootreason", "dc_chg");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "5") {
            property_override("ro.boot.bootreason", "usb_chg");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "6") {
            property_override("ro.boot.bootreason", "pon1");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "7") {
            property_override("ro.boot.bootreason", "cblpwr");
            property_override("ro.vendor.alarm_boot", "false");
        }
        else if (Trim(boot_reason) == "8") {
            property_override("ro.boot.bootreason", "kpdpwr");
            property_override("ro.vendor.alarm_boot", "false");
        }
    }
    else {
        LOG(ERROR) << "Unable to read bootreason from " << boot_reason_file;
    }
}

void vendor_load_properties()
{
    // fingerprint
    property_override("ro.build.description", "redfin-user 11 RQ2A.210405.005 7181113 release-keys");
    property_override_multi("ro.build.fingerprint", "ro.vendor.build.fingerprint","ro.bootimage.build.fingerprint", "google/redfin/redfin:11/RQ2A.210405.005/7181113:user/release-keys");
    init_target_properties();
    init_fingerprint_properties();
    load_dalvikvm_properties();
}
