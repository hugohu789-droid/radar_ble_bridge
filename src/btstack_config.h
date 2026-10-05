#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

/*
 * BTstack configuration for the radar BLE bridge (BLE peripheral only).
 * Based on the pico-examples BLE configuration, trimmed to what a
 * single-connection GATT server needs.
 */

#ifndef ENABLE_BLE
#error "Link pico_btstack_ble"
#endif

/* ---- Features ---- */
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#define ENABLE_PRINTF_HEXDUMP

/* Peripheral only: no GATT client */
#define MAX_NR_GATT_CLIENTS 0

/* ---- Buffers and sizes ---- */
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
/*
 * Largest ACL payload. 251 + 4 bytes of L2CAP header lets the ATT MTU
 * go up to 247, the usual maximum phones negotiate (used in phase 3).
 */
#define HCI_ACL_PAYLOAD_SIZE (251 + 4)
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4

/* One phone at a time */
#define MAX_NR_HCI_CONNECTIONS 1

#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 16
#define MAX_NR_LE_DEVICE_DB_ENTRIES 16

/* Limit controller buffers to avoid overrunning the cyw43 shared bus */
#define MAX_NR_CONTROLLER_ACL_BUFFERS 3
#define MAX_NR_CONTROLLER_SCO_PACKETS 3

/* Controller-to-host flow control, also to protect the cyw43 bus */
#define ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#define HCI_HOST_ACL_PACKET_LEN (251 + 4)
#define HCI_HOST_ACL_PACKET_NUM 3
#define HCI_HOST_SCO_PACKET_LEN 120
#define HCI_HOST_SCO_PACKET_NUM 3

/* Bonding keys stored in flash (used in phase 4) */
#define NVM_NUM_DEVICE_DB_ENTRIES 16
#define NVM_NUM_LINK_KEYS 16

/* No malloc for BTstack, so the ATT database has a fixed size */
#define MAX_ATT_DB_SIZE 512

/* ---- HAL ---- */
#define HAVE_EMBEDDED_TIME_MS
#define HAVE_ASSERT
#define HCI_RESET_RESEND_TIMEOUT_MS 1000
#define ENABLE_SOFTWARE_AES128
#define ENABLE_MICRO_ECC_FOR_LE_SECURE_CONNECTIONS

#endif /* BTSTACK_CONFIG_H */
