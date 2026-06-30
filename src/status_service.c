#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

// Custom BLE UUIDs
// Status Service:   5F3A0000-D0E1-4D9A-8E40-12866D57AA42
// Layer Status Chrc: 5F3A0001-D0E1-4D9A-8E40-12866D57AA42
// Split Link Chrc:   5F3A0002-D0E1-4D9A-8E40-12866D57AA42

#define BT_UUID_ZMK_STATUS_VAL \
    BT_UUID_128_ENCODE(0x5F3A0000, 0xD0E1, 0x4D9A, 0x8E40, 0x12866D57AA42)
#define BT_UUID_ZMK_STATUS_LAYER_VAL \
    BT_UUID_128_ENCODE(0x5F3A0001, 0xD0E1, 0x4D9A, 0x8E40, 0x12866D57AA42)
#define BT_UUID_ZMK_STATUS_SPLIT_VAL \
    BT_UUID_128_ENCODE(0x5F3A0002, 0xD0E1, 0x4D9A, 0x8E40, 0x12866D57AA42)

static struct bt_uuid_128 status_uuid = BT_UUID_INIT_128(BT_UUID_ZMK_STATUS_VAL);
static struct bt_uuid_128 layer_uuid = BT_UUID_INIT_128(BT_UUID_ZMK_STATUS_LAYER_VAL);
static struct bt_uuid_128 split_uuid = BT_UUID_INIT_128(BT_UUID_ZMK_STATUS_SPLIT_VAL);

static uint8_t active_layer = 0;
static uint8_t split_connected = 0;

static ssize_t read_layer(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &active_layer, sizeof(active_layer));
}

static ssize_t read_split(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &split_connected, sizeof(split_connected));
}

static void status_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    // This callback is invoked when a client subscribes/unsubscribes to notifications
}

// Register the GATT Service
BT_GATT_SERVICE_DEFINE(zmk_status_svc,
    BT_GATT_PRIMARY_SERVICE(&status_uuid),
    
    // Active Layer Characteristic (Attribute Index 2 in this service block)
    BT_GATT_CHARACTERISTIC(&layer_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_layer, NULL, &active_layer),
    BT_GATT_CCC(status_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    
    // Split Connection Characteristic (Attribute Index 5 in this service block)
    BT_GATT_CHARACTERISTIC(&split_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_split, NULL, &split_connected),
    BT_GATT_CCC(status_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

// Unified handler for ZMK Event Manager callbacks
static int status_event_handler(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
    if (layer_ev) {
        active_layer = zmk_keymap_highest_active_layer();
        // Notify all registered BLE subscribers of the layer change
        // &zmk_status_svc.attrs[2] is the characteristic value attribute for layer index
        bt_gatt_notify(NULL, &zmk_status_svc.attrs[2], &active_layer, sizeof(active_layer));
        return 0;
    }
    
    const struct zmk_split_peripheral_status_changed *split_ev = as_zmk_split_peripheral_status_changed(eh);
    if (split_ev) {
        split_connected = split_ev->connected ? 1 : 0;
        // Notify all registered BLE subscribers of the split connection change
        // &zmk_status_svc.attrs[5] is the characteristic value attribute for split status
        bt_gatt_notify(NULL, &zmk_status_svc.attrs[5], &split_connected, sizeof(split_connected));
        return 0;
    }
    
    return 0;
}

// Set up the subscriptions on the event bus
ZMK_LISTENER(status_listener, status_event_handler);
ZMK_SUBSCRIPTION(status_listener, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(status_listener, zmk_split_peripheral_status_changed);
