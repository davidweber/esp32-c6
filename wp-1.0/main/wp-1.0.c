#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "nvs_flash.h"

#define BUTTON_GPIO GPIO_NUM_22
#define LED_GPIO GPIO_NUM_4
#define GATTS_TAG "BLE_BUTTON"

static uint16_t ble_conn_id = 0;
static bool is_connected = false;
static const char *message = "Button Pressed!";
static QueueHandle_t gpio_evt_queue = NULL;

void button_handler(void *arg);

// BLE characteristics and service UUIDs
static const uint16_t SERVICE_UUID = 0x00FF;
static const uint16_t CHAR_UUID = 0xFF01;

static esp_gatt_char_prop_t char_property = ESP_GATT_CHAR_PROP_BIT_NOTIFY;

// GPIO ISR handler
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Task to handle button press
void button_task(void *arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            gpio_set_level(LED_GPIO, 1);  // Turn on LED
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(LED_GPIO, 0);  // Turn off LED
            if (is_connected) {
                esp_ble_gatts_send_indicate(ble_conn_id, 0, 0, strlen(message), (uint8_t *)message, false);
            }
        }
    }
}

// Event handler for GATT server
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Client connected");
            is_connected = true;
            ble_conn_id = param->connect.conn_id;
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Client disconnected");
            is_connected = false;
            esp_ble_gap_start_advertising(&adv_params);
            break;
        default:
            break;
    }
}

void app_main() {
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0);

    // GPIO configuration for button and LED
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << LED_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Create a queue to handle button interrupts
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Install GPIO ISR handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *) BUTTON_GPIO);

    // Create a task to handle button press
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
}

