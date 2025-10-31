/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "HeartRate.hpp"

//extern "C" void send_heart_rate_indication(void);

/* Private variables */
static uint8_t heart_rate;

/* Public functions */
uint8_t HeartRate::get_heart_rate(void) { return heart_rate; }

void HeartRate::update_heart_rate(void) { heart_rate = 60 + (uint8_t)(esp_random() % 21); }

void HeartRate::heart_rate_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "heart rate task has been started!");

    /* Loop forever */
    while (1) {
        /* Update heart rate value every 1 second */
        update_heart_rate();
        ESP_LOGI(TAG, "heart rate updated to %d", get_heart_rate());

        //send_heart_rate_indication();

        /* Sleep */
        vTaskDelay(HEART_RATE_TASK_PERIOD);
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}