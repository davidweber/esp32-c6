/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "battery.h"
#include "common.h"
#include "gatt_svc.h"

/* Private function declarations */
static int button_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);
static int battery_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
/* Private variables */
static const ble_uuid16_t button_svc_uuid = BLE_UUID16_INIT(0x280D);

static uint8_t button_val[2] = {0};
static uint16_t button_chr_val_handle;
static const ble_uuid16_t button_uuid = BLE_UUID16_INIT(0x2A37);

static uint16_t button_chr_conn_handle = 0;
static bool button_chr_conn_handle_inited = false;
static bool button_ind_status = false;

static uint16_t battery_chr_conn_handle = 0;
static uint16_t battery_chr_val_handle;
static bool battery_chr_conn_handle_inited = false;
static bool battery_ind_status = false;
static uint16_t battery_level = 0;

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Battery service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180F), // Battery Service
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A19), // Battery Level
                    .access_cb = battery_access, // Handled automatically by
                                                 // NimBLE for notifications
                    .flags = BLE_GATT_CHR_F_INDICATE,
                    .val_handle = &battery_chr_val_handle,
                },
                {
                    0, // No more characteristics in this service
                }},
    },
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &button_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* button characteristic */
              .uuid = &button_uuid.u,
              .access_cb = button_access,
              .flags = BLE_GATT_CHR_F_INDICATE,
              .val_handle = &button_chr_val_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},
    {
        0, /* No more services. */
    },
};
//------------------------------------------------------------------------------

static int button_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
  /* Local variables */
  int rc;

  /* Handle access events */
  switch (ctxt->op)
  {

  /* Read characteristic event */
  case BLE_GATT_ACCESS_OP_READ_CHR:
    /* Verify connection handle */
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
      ESP_LOGI(TAG, "button characteristic read; conn_handle=%d attr_handle=%d",
               conn_handle, attr_handle);
    }
    else
    {
      ESP_LOGI(TAG,
               "button characteristic read by nimble stack; attr_handle=%d",
               attr_handle);
    }

    /* Verify attribute handle */
    if (attr_handle == button_chr_val_handle)
    {
      /* Update access buffer value */
      rc = os_mbuf_append(ctxt->om, &button_val, sizeof(button_val));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    goto error;

  /* Unknown event */
  default:
    goto error;
  }

error:
  ESP_LOGE(TAG,
           "unexpected access operation to button characteristic, opcode: %d",
           ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

static int battery_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)

{
  /* Local variables */
  int rc;

  /* Handle access events */
  switch (ctxt->op)
  {

  /* Read characteristic event */
  case BLE_GATT_ACCESS_OP_READ_CHR:
    /* Verify connection handle */
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
      ESP_LOGI(TAG,
               "battery characteristic read; conn_handle=%d attr_handle=%d",
               conn_handle, attr_handle);
    }
    else
    {
      ESP_LOGI(TAG,
               "battery characteristic read by nimble stack; attr_handle=%d",
               attr_handle);
    }

    /* Verify attribute handle */
    if (attr_handle == battery_chr_val_handle)
    {
      /* Update access buffer value */
      rc = os_mbuf_append(ctxt->om, &battery_level, sizeof(battery_level));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    goto error;

  /* Unknown event */
  default:
    goto error;
  }

error:
  ESP_LOGE(
      TAG,
      "unexpected access operation to heart rate characteristic, opcode: %d",
      ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

//------------------------------------------------------------------------------

void send_battery_indication(void)
{
  battery_level = battery_get_percentage();
  ESP_LOGI(TAG, "battery level = %d", battery_level);
  if (battery_ind_status && battery_chr_conn_handle_inited)
  {
    ble_gatts_indicate(battery_chr_conn_handle, battery_chr_val_handle);
    ESP_LOGI(TAG, "battery indication sent!");
  }
}

//------------------------------------------------------------------------------

void send_button_indication(uint8_t pressed)
{
  button_val[1] = pressed;
  button_val[0] = battery_get_percentage();
  if (button_ind_status && button_chr_conn_handle_inited)
  {
    ble_gatts_indicate(button_chr_conn_handle,
                       button_chr_val_handle); //, pressed, 1);
    ESP_LOGI(TAG, "button indication sent, battery level = %d!", button_val[0]);
  }
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
  /* Local variables */
  char buf[BLE_UUID_STR_LEN];

  /* Handle GATT attributes register events */
  switch (ctxt->op)
  {

  /* Service register event */
  case BLE_GATT_REGISTER_OP_SVC:
    ESP_LOGD(TAG, "registered service %s with handle=%d",
             ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
    break;

  /* Characteristic register event */
  case BLE_GATT_REGISTER_OP_CHR:
    ESP_LOGD(TAG,
             "registering characteristic %s with "
             "def_handle=%d val_handle=%d",
             ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
             ctxt->chr.def_handle, ctxt->chr.val_handle);
    break;

  /* Descriptor register event */
  case BLE_GATT_REGISTER_OP_DSC:
    ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
             ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
    break;

  /* Unknown event */
  default:
    assert(0);
    break;
  }
}

/*
 *  GATT server subscribe event callback
 *      1. Update heart rate subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
  /* Check connection handle */
  if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE)
  {
    ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
             event->subscribe.conn_handle, event->subscribe.attr_handle);
  }
  else
  {
    ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
             event->subscribe.attr_handle);
  }

#if 0
    /* Check attribute handle */
    if (event->subscribe.attr_handle == heart_rate_chr_val_handle) {
        /* Update heart rate subscription status */
        heart_rate_chr_conn_handle = event->subscribe.conn_handle;
        heart_rate_chr_conn_handle_inited = true;
        heart_rate_ind_status = event->subscribe.cur_indicate;
    }
#endif
  /* Check attribute handle */
  if (event->subscribe.attr_handle == button_chr_val_handle)
  {
    /* Update button subscription status */
    button_chr_conn_handle = event->subscribe.conn_handle;
    button_chr_conn_handle_inited = true;
    button_ind_status = event->subscribe.cur_indicate;
  }
  /* Check attribute handle */
  if (event->subscribe.attr_handle == battery_chr_val_handle)
  {
    /* Update battery subscription status */
    battery_chr_conn_handle = event->subscribe.conn_handle;
    battery_chr_conn_handle_inited = true;
    battery_ind_status = event->subscribe.cur_indicate;
  }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void)
{
  /* Local variables */
  int rc;

  /* 1. GATT service initialization */
  ble_svc_gatt_init();

  /* 2. Update GATT services counter */
  rc = ble_gatts_count_cfg(gatt_svr_svcs);
  if (rc != 0)
  {
    return rc;
  }

  /* 3. Add GATT services */
  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0)
  {
    return rc;
  }

  return 0;
}
