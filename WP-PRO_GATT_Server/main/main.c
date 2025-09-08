/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "heart_rate.h"
#include "driver/gpio.h"
#include "reg.h"
#include "reg_esp32c6_gpio.h"

#define LED_GPIO 5
#define BUTTON_GPIO 22

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
static void blink_task(void *param);
static void button_task(void *param);
void heart_rate_task(void *param);

//------------------------------------------------------------------------------

/* Private functions */
/*
 *  Stack event callback functions
 *    - on_stack_reset is called when host resets BLE stack due to errors
 *    - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
  /* On reset, print reset reason to console */
  ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

//------------------------------------------------------------------------------

static void on_stack_sync(void) {
  /* On stack sync, do advertising initialization */
  adv_init();
}

//------------------------------------------------------------------------------

static void nimble_host_config_init(void) {
  /* Set host callbacks */
  ble_hs_cfg.reset_cb = on_stack_reset;
  ble_hs_cfg.sync_cb = on_stack_sync;
  ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  /* Store host configuration */
  ble_store_config_init();
}

//------------------------------------------------------------------------------

static void nimble_host_task(void *param) {
  /* Task entry log */
  ESP_LOGI(TAG, "nimble host task has been started!");

  /* This function won't return until nimble_port_stop() is executed */
  nimble_port_run();

  /* Clean up at exit */
  vTaskDelete(NULL);
}

//------------------------------------------------------------------------------

void heart_rate_task(void *param)
{
  /* Task entry log */
  ESP_LOGI(TAG, "heart rate task has been started!");

  /* Loop forever */
  while (1)
  {
    /* Update heart rate value every 1 second */
    update_heart_rate();
    ESP_LOGI(TAG, "heart rate updated to %d", get_heart_rate());

    /* Send heart rate indication if enabled */
    send_heart_rate_indication();

    /* Sleep */
    vTaskDelay(HEART_RATE_TASK_PERIOD);
  }

  /* Clean up at exit */
  vTaskDelete(NULL);
}

//------------------------------------------------------------------------------

static void blink_task(void *param)
{
  // Configure GPIO 4 as an output pin
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

//  REG_S1(GPIO_ENABLE, D5);
//  REG_S1(GPIO_OUT_SET_ENABLE, D5);
//  REG_S1(GPIO_OUT_CLR_ENABLE, D5);

  while (1)
  {
    ESP_LOGI(TAG, "blink\n");
    // Turn the LED on
    REG_S1(GPIO_OUT_SET, D5);
    vTaskDelay(20 / portTICK_PERIOD_MS);  // Delay for 1 second

    // Turn the LED off
    REG_S1(GPIO_OUT_CLR, D5);
    vTaskDelay(30000 / portTICK_PERIOD_MS);  // Delay for 1 second
  }
  vTaskDelete(NULL);
}

//------------------------------------------------------------------------------

static void button_task(void *param)
{
  gpio_reset_pin(BUTTON_GPIO);
  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);

  while (1)
  {
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay for 200 ms
    if (REG_R(GPIO_IN, D22) == 0)
    {
      ESP_LOGI(TAG, "button pressed\n");
      send_button_indication(1u);
      REG_S1(GPIO_OUT_SET, D5);
      // wait for button release
      while (REG_R(GPIO_IN, D22) == 0)
      { 
//        send_heart_rate_indication();
//        send_button_indication(1u);
//        esp_ble_gatts_send_indicate(gatts_if, conn_id, gatt_handle_table[2], sizeof(value), value, true);
        vTaskDelay(5/portTICK_PERIOD_MS);
      }
      ESP_LOGI(TAG, "button released\n");
      REG_S1(GPIO_OUT_CLR, D5);
      send_button_indication(0u);
    }
  }
  vTaskDelete(NULL);
}

//------------------------------------------------------------------------------

void app_main(void) {
    /* Local variables */
    int rc;
    esp_err_t ret;

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* GATT server initialization */
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
//    xTaskCreate(heart_rate_task, "Heart Rate", 4*1024, NULL, 5, NULL);
    xTaskCreate(blink_task, "Blink", 4*1024, NULL, 5, NULL);
    xTaskCreate(button_task, "Button", 4*1024, NULL, 5, NULL);
    return;
}
