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
#include "hpm_romapi.h"
#include <stdio.h>

/* DFU functional descriptor is 9 bytes */
#define DFU_DESC_TOTAL_LEN (9 + 9)
#define USB_CONFIG_SIZE    (9 + DFU_DESC_TOTAL_LEN)

static char flash_internal_desc_str[128];

/*
 * USB iSerialNumber, filled by serial_str_build().
 *
 * It MUST be unique per device instance: Windows keys the instance id on
 * USB\VID_34B7&PID_0003\<serial> and will not enumerate a second board whose
 * serial collides, so a hardcoded string breaks as soon as two boards are
 * attached at the same time (and makes `dfu-util -S <serial>` useless).
 *
 * Derived from the top 96 bits of the chip UUID in the OTP shadow
 * (OTP_SOC_UUID_IDX = 88) via the boot ROM API -- the same call the SDK's
 * TinyUSB BSP uses in board_get_unique_id().  Words 88..90 match what the
 * application uses, so one board reports the same serial in both modes.
 */
#define OTP_UUID_WORD_IDX (88U) /* hpm_soc_feature.h: OTP_SOC_UUID_IDX */

static char serial_str[25];

static void serial_str_build(void)
{
    static const char hex[] = "0123456789ABCDEF";
    uint32_t i;

    for (i = 0U; i < 3U; i++) {
        const uint32_t word =
            ROM_API_TABLE_ROOT->otp_driver_if->read_from_shadow(OTP_UUID_WORD_IDX + i);
        uint32_t n;

        for (n = 0U; n < 8U; n++) {
            serial_str[8U * i + n] = hex[(word >> (28U - 4U * n)) & 0xFU];
        }
    }
    serial_str[24] = '\0';
}

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
    serial_str,               /* filled by serial_str_build() */
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

/* ========================================================================
 * Microsoft OS 1.0 descriptors (WCID) -> Windows binds WinUSB to the DFU
 * interface automatically, so dfu-util can claim it without a manual Zadig
 * step (the same mechanism the candleLight application uses).
 *
 * Windows queries:
 *   GET_DESCRIPTOR(String, index 0xEE)      -> msos_string
 *   vendor request bRequest=0x20 wIndex=4   -> msos_compat_id
 *   vendor request bRequest=0x20 wIndex=5   -> msos_ext_prop (wValue = 0)
 * ======================================================================== */
#define DFU_WINUSB_VENDOR_CODE 0x20U

/* Not the same GUID as the gs_usb (candleLight) interface, so that host tools
 * can tell the bootloader apart from the application. */
#define DFU_INTERFACE_GUID "{7a3e5c91-2b48-4d6f-a1e3-58c9d0b47f26}"

/* Microsoft OS String Descriptor, index 0xEE ("MSFT100" + vendor code) */
static const uint8_t msos_string[] = {
    0x12, 0x03,
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00,
    '1', 0x00, '0', 0x00, '0', 0x00,
    DFU_WINUSB_VENDOR_CODE,
    0x00,
};

/* Compatible ID Feature Descriptor: one interface (0) -> WINUSB */
static const uint8_t msos_compat_id[] = {
    0x28, 0x00, 0x00, 0x00, /* dwLength = 16 + 24 * 1 */
    0x00, 0x01,             /* bcdVersion 1.0 */
    0x04, 0x00,             /* wIndex 0x0004 */
    0x01,                   /* bCount */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved[7] */
    0x00,                   /* bFirstInterfaceNumber */
    0x01,                   /* reserved1 */
    0x57, 0x49, 0x4E, 0x55, /* compatibleID "WINUSB\0\0" */
    0x53, 0x42, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, /* subCompatibleID */
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved2[6] */
};

/*
 * Extended Properties Feature Descriptor (DeviceInterfaceGUIDs).
 * Layout mirrors the proven candleLight blob: 10 byte header + one property
 * section holding a REG_MULTI_SZ.  It is assembled at init time from the plain
 * ASCII strings above so the UTF-16LE conversion cannot be mistyped.
 */
#define MSOS_EXT_PROP_LEN (0x92U)
static uint8_t msos_ext_prop[MSOS_EXT_PROP_LEN];

/* Guard the descriptor size arithmetic below against a future reword of
 * prop_name / DFU_INTERFACE_GUID: it must still total MSOS_EXT_PROP_LEN. */
_Static_assert((10U + 4U + 4U + 2U + (sizeof("DeviceInterfaceGUIDs") * 2U) + 4U +
                ((sizeof(DFU_INTERFACE_GUID) + 1U) * 2U)) == MSOS_EXT_PROP_LEN,
               "msos_ext_prop size mismatch");

/* Returned for wValue != 0: a valid but empty property set. */
static const uint8_t msos_ext_prop_empty[] = {
    0x0a, 0x00, 0x00, 0x00, /* dwLength = 10 */
    0x00, 0x01,             /* bcdVersion 1.0 */
    0x05, 0x00,             /* wIndex 0x0005 */
    0x00, 0x00,             /* bCount = 0 */
};

/* CherryUSB indexes this array with setup->wValue, so keep two entries. */
static const uint8_t *msos_ext_prop_list[2];

static const struct usb_msosv1_descriptor dfu_msosv1 = {
    .string = msos_string,
    .vendor_code = DFU_WINUSB_VENDOR_CODE,
    .compat_id = msos_compat_id,
    .comp_id_property = msos_ext_prop_list,
};

static void msos_ext_prop_build(void)
{
    static const char prop_name[] = "DeviceInterfaceGUIDs";
    static const char guid[] = DFU_INTERFACE_GUID;
    const uint32_t name_bytes = (uint32_t)sizeof(prop_name) * 2U;      /* + NUL */
    const uint32_t data_bytes = ((uint32_t)sizeof(guid) + 1U) * 2U;    /* + 2 NUL */
    const uint32_t section_len = 4U + 4U + 2U + name_bytes + 4U + data_bytes;
    uint32_t p = 0U;
    uint32_t i;

    msos_ext_prop[p++] = (uint8_t)(10U + section_len);
    msos_ext_prop[p++] = (uint8_t)((10U + section_len) >> 8);
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U; /* bcdVersion 1.0 */
    msos_ext_prop[p++] = 0x01U;
    msos_ext_prop[p++] = 0x05U; /* wIndex 0x0005 */
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x01U; /* bCount = 1 */
    msos_ext_prop[p++] = 0x00U;

    msos_ext_prop[p++] = (uint8_t)(section_len);
    msos_ext_prop[p++] = (uint8_t)(section_len >> 8);
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x07U; /* dwPropertyDataType: REG_MULTI_SZ */
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = (uint8_t)(name_bytes);
    msos_ext_prop[p++] = (uint8_t)(name_bytes >> 8);

    for (i = 0U; i < (uint32_t)sizeof(prop_name); i++) {
        msos_ext_prop[p++] = (uint8_t)prop_name[i];
        msos_ext_prop[p++] = 0x00U;
    }

    msos_ext_prop[p++] = (uint8_t)(data_bytes);
    msos_ext_prop[p++] = (uint8_t)(data_bytes >> 8);
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;

    for (i = 0U; i < (uint32_t)sizeof(guid); i++) {
        msos_ext_prop[p++] = (uint8_t)guid[i];
        msos_ext_prop[p++] = 0x00U;
    }
    /* REG_MULTI_SZ terminator */
    msos_ext_prop[p++] = 0x00U;
    msos_ext_prop[p++] = 0x00U;

    msos_ext_prop_list[0] = msos_ext_prop;
    msos_ext_prop_list[1] = msos_ext_prop_empty;
}

/* ========== Descriptor Registration ========== */
const struct usb_descriptor dfu_descriptor = {
    .device_descriptor_callback          = device_descriptor_cb,
    .config_descriptor_callback          = config_descriptor_cb,
    .device_quality_descriptor_callback  = device_quality_descriptor_cb,
    .other_speed_descriptor_callback     = config_descriptor_cb,
    .string_descriptor_callback          = string_descriptor_cb,
    /* MS OS 1.0 (WCID) so Windows installs WinUSB for the DFU interface */
    .msosv1_descriptor = &dfu_msosv1,
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

    /* Chip UUID -> USB iSerialNumber (must be unique per board) */
    serial_str_build();

    /* Assemble the WCID extended properties (DeviceInterfaceGUIDs) */
    msos_ext_prop_build();

    usbd_desc_register(busid, &dfu_descriptor);
    usbd_add_interface(busid, usbd_dfu_init_intf(&intf0));
    usbd_initialize(busid, reg_base, usbd_event_handler);
}
