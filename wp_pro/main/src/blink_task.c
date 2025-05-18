#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "blink_task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "reg.h"
#include "reg_esp32c6_gpio.h"

//------------------------------------------------------------------------------

void blink_task(void *param)
{
  // Configure GPIO 5 as an output pin
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

//  REG_S1(GPIO_ENABLE, D5);
//  REG_S1(GPIO_OUT_SET_ENABLE, D5);
//  REG_S1(GPIO_OUT_CLR_ENABLE, D5);

  while (1)
  {
    ESP_LOGI(BLINK_TASK_TAG, "blink\n");
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

void blink_task_create(void)
{
    xTaskCreate(blink_task, "blink_task", 4*1024, NULL, 5, NULL);
}
