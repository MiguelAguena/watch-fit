#pragma once
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMMON_H
#define COMMON_H

/* Includes */
/* STD APIs */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

/* ESP APIs */
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* FreeRTOS APIs */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* NimBLE stack APIs */
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* Defines */
#define TAG "Node"
#define DEVICE_NAME "WATCH-FIT_NODE"

#endif // COMMON_H

#ifdef __cplusplus
}
#endif