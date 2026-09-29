/*
 * DFU USB descriptors for HPM DFU Bootloader
 * DfuSe protocol path: uses SDK hpm_dfu_port.c for flash callbacks.
 */
#include "usbd_core.h"
#include "usbd_dfu.h"
#include "usb_dfu.h"
#include "boot_log.h"
#include "board.h"
#include "usb_config.h"
#include <stdio.h>

/* DFU functional descriptor is 9 bytes */
#define DFU_DESC_TOTAL_LEN (9 + 9)
#define USB_CONFIG_SIZE    (9 + DFU_DESC_TOTAL_LEN)

static char flash_internal_desc_str[128];

/* ========== Device Descriptor ========== */
static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0200, 0x01)
};

/* ========== Config Descriptor (DFU-only, single alt setting) ========== */
static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, 0xC0, USBD_MAX_POWER),
    /* DFU Interface 0, Alt 0, DFU Mode */
    0x09, 0x04, 0x00, 0x00, 0x00,
    0xFE, 0x01, 0x02,    /* class=APP_SPECIFIC, subclass=DFU, protocol=DFU_MODE */
    0x04,                /* iInterface = 4 -> DfuSe memory layout string */
    /* DFU Functional Descriptor */
    0x09, 0x21,          /* bLength, bDescriptorType = DFU_FUNCTIONAL */
    0x0B,                /* bmAttributes: CanDnload | CanUpload | WillDetach */
    0xFF, 0x00,          /* wDetachTimeout = 255ms */
    0x00, 0x10,          /* wTransferSize = 4096 */
    0x1A, 0x01,          /* bcdDFU = 1.1a */
};

/* ========== String Descriptors ========== */
/* iInterface (index 4) is DfuSe format — computed at init time. */
static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },
    "HPMicro",
    "HPMicro Dfu",
    "2026092600",
    flash_internal_desc_str,  /* filled by dfu_boot_init */
};
static const uint8_t *device_descriptor_cb(uint8_t speed)
{
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_cb(uint8_t speed)
{
    (void)speed;
    return config_descriptor;
}

static const char *string_descriptor_cb(uint8_t speed, uint8_t index)
{
    (void)speed;
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

/* ========== Device Qualifier (USB 2.0) ========== */
static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40,
    0x01,
    0x00,
};

static const uint8_t *device_quality_descriptor_cb(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

/* ========== BOS Descriptor (USB 2.0 Extension) ========== */
static const uint8_t bos_descriptor_data[] = {
    /* BOS Header */
    0x05,
    USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE,
    0x0C, 0x00,
    0x01,
    /* USB 2.0 Extension Capability */
    0x07,
    0x10,
    0x02,
    0x02, 0x00, 0x00, 0x00,
};

static const struct usb_bos_descriptor bos_descriptor = {
    .string = bos_descriptor_data,
    .string_len = sizeof(bos_descriptor_data),
};

/* ========== Descriptor Registration ========== */
const struct usb_descriptor dfu_descriptor = {
    .device_descriptor_callback          = device_descriptor_cb,
    .config_descriptor_callback          = config_descriptor_cb,
    .device_quality_descriptor_callback  = device_quality_descriptor_cb,
    .other_speed_descriptor_callback     = config_descriptor_cb,
    .string_descriptor_callback          = string_descriptor_cb,
    .msosv2_descriptor = NULL,
    .bos_descriptor    = &bos_descriptor,
};

/* ========== USB Init ========== */
static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
    case USBD_EVENT_RESET:     BOOT_PRINTF("[USB] RESET\r\n"); break;
    case USBD_EVENT_CONNECTED: BOOT_PRINTF("[USB] CONNECTED\r\n"); break;
    case USBD_EVENT_CONFIGURED:BOOT_PRINTF("[USB] CONFIGURED\r\n"); break;
    default: break;
    }
}

static struct usbd_interface intf0;

void dfu_boot_init(uint8_t busid, uintptr_t reg_base)
{
    /* Build DfuSe interface string at runtime (like SDK sample) */
    uint32_t dfu_app_size = BOARD_FLASH_SIZE
                          - (USBD_DFU_APP_DEFAULT_ADD - BOARD_FLASH_BASE_ADDRESS);
    (void)snprintf(flash_internal_desc_str, sizeof(flash_internal_desc_str),
                   "@Internal Flash /0x%08lX/%lu*%luKg",
                   (unsigned long)USBD_DFU_APP_DEFAULT_ADD,
                   (unsigned long)(dfu_app_size / (16 * 1024)),
                   (unsigned long)(16));

    usbd_desc_register(busid, &dfu_descriptor);
    usbd_add_interface(busid, usbd_dfu_init_intf(&intf0));
    usbd_initialize(busid, reg_base, usbd_event_handler);
}
