#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "blink_task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "reg.h"
#include "reg_esp32c6_gpio.h"

//------------------------------------------------------------------------------

//void button_task(void* param)
//{
//  gpio_reset_pin(BUTTON_GPIO);
//  gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
//
//  while (1)
//  {
//    //wait for button press
//    while (REG_R(GPIO_IN, D22) != 0)
//    {
//      vTaskDelay(50 / portTICK_PERIOD_MS);  // Delay for 200 ms
//    }
//
//    ESP_LOGI(GATTS_TABLE_TAG, "button pressed\n");
//
//    // turn on LED
//    REG_S1(GPIO_OUT_SET, D5);
//
//    // send indication to client
//    button_send_state(1u);
//
//    // wait for button release
//    while(REG_R(GPIO_IN, D22) == 0)
//    {
//      vTaskDelay(50 / portTICK_PERIOD_MS);
//    }
//
//    // send indication to client
//    button_send_state(0u);
//
//    // turn off LED
//    REG_S1(GPIO_OUT_CLR, D5);
//
//    ESP_LOGI(GATTS_TABLE_TAG, "button released\n");
//    
//  }
//  vTaskDelete(NULL);
//}
//
////------------------------------------------------------------------------------
//
//void button_task_create(void)
//{
//  xTaskCreate(button_task, "button_task", 4x1024, NULL, 5, NULL);
//}

