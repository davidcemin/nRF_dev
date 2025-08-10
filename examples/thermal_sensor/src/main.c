/* main.c - Application main entry point
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/drivers/sensor.h>

/* ---------- GATT: Environmental Sensing (Temperature 0x2A6E) ---------- */

static int16_t gatt_temp_centi;       /* signed 0.01 °C units */
static bool    temp_ntf_enabled;

static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    temp_ntf_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static ssize_t temp_read(struct bt_conn *c, const struct bt_gatt_attr *a,
                         void *buf, uint16_t len, uint16_t off)
{
    const int16_t *val = a->user_data;
    return bt_gatt_attr_read(c, a, buf, len, off, val, sizeof(*val));
}

/* attrs: [0]=primary, [1]=char decl, [2]=value, [3]=CCC */
BT_GATT_SERVICE_DEFINE(env_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
    BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        temp_read, NULL, &gatt_temp_centi),
    BT_GATT_CCC(temp_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

/* ---------- DS18B20 (1‑Wire) ---------- */

#define DS18B20_NODE DT_NODELABEL(ds18b20_0)
const struct device *const ds18b20 = DEVICE_DT_GET(DS18B20_NODE);

/* 2B company ID (Nordic 0x0059) + 2B temperature (int16, 0.01 °C, LE) */
static uint8_t mfg_buf[4] = { 0x59, 0x00, 0x00, 0x00 };

/* ---------- Advertising data ---------- */

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    /* Advertise Environmental Sensing Service so the app recognizes it */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_ESS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
    { .type = BT_DATA_MANUFACTURER_DATA, .data_len = sizeof(mfg_buf), .data = mfg_buf },
};

#if !defined(CONFIG_BT_EXT_ADV)
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
#endif

/* ---------- Connection callbacks ---------- */

static ATOMIC_DEFINE(state, 2U);
#define STATE_CONNECTED    1U
#define STATE_DISCONNECTED 2U

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
    } else {
        printk("Connected\n");
        (void)atomic_set_bit(state, STATE_CONNECTED);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
    (void)atomic_set_bit(state, STATE_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_cbs) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Optional pairing-cancel callback (no passkey etc. here) */
static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Pairing cancelled: %s\n", addr);
}
static struct bt_conn_auth_cb auth_cb_display = {
    .cancel = auth_cancel,
};

/* ---------- Optional LED blink ---------- */
#if defined(CONFIG_GPIO)
#define LED0_NODE DT_ALIAS(led0)
#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
#include <zephyr/drivers/gpio.h>
#define HAS_LED 1
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define BLINK_ONOFF K_MSEC(500)
static struct k_work_delayable blink_work;
static bool led_is_on;

static void blink_timeout(struct k_work *work)
{
    led_is_on = !led_is_on;
    gpio_pin_set(led.port, led.pin, (int)led_is_on);
    k_work_schedule(&blink_work, BLINK_ONOFF);
}

static int blink_setup(void)
{
    if (!gpio_is_ready_dt(&led)) {
        printk("LED device not ready\n");
        return -EIO;
    }
    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (err) {
        printk("LED configure failed: %d\n", err);
        return err;
    }
    k_work_init_delayable(&blink_work, blink_timeout);
    return 0;
}
static void blink_start(void)
{
    led_is_on = false;
    gpio_pin_set(led.port, led.pin, (int)led_is_on);
    k_work_schedule(&blink_work, BLINK_ONOFF);
}
static void blink_stop(void)
{
    struct k_work_sync s;
    k_work_cancel_delayable_sync(&blink_work, &s);
    led_is_on = true;
    gpio_pin_set(led.port, led.pin, (int)led_is_on);
}
#endif
#endif /* CONFIG_GPIO */

/* ============================ main ============================ */

int main(void)
{
    int err;

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    printk("Bluetooth initialized\n");

    if (!device_is_ready(ds18b20)) {
        printk("DS18B20 not ready!\n");
    }

    bt_conn_auth_cb_register(&auth_cb_display);

#if !defined(CONFIG_BT_EXT_ADV)
    printk("Starting Legacy Advertising (connectable & scannable)\n");
    err = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising start failed (%d)\n", err);
        return 0;
    }
#else
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0U,
        .secondary_max_skip = 0U,
        .options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_CODED),
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer = NULL,
    };
    struct bt_le_ext_adv *adv;
    printk("Creating extended advertising set\n");
    err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
    if (err) {
        printk("Ext adv create failed (%d)\n", err);
        return 0;
    }
    err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Ext adv set data failed (%d)\n", err);
        return 0;
    }
    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("Ext adv start failed (%d)\n", err);
        return 0;
    }
#endif

    printk("Advertising started\n");

#if defined(HAS_LED)
    if (blink_setup() == 0) {
        blink_start();
    }
#endif

    while (1) {
        k_sleep(K_SECONDS(1));

        /* ---- Read DS18B20 ---- */
        struct sensor_value t;
        if (sensor_sample_fetch(ds18b20) == 0 &&
            sensor_channel_get(ds18b20, SENSOR_CHAN_AMBIENT_TEMP, &t) == 0) {

            /* t.val1 = whole °C, t.val2 = micro °C -> centi °C (rounded) */
            int32_t centi = t.val1 * 100;
            centi += (t.val2 >= 0 ? (t.val2 + 5000) : (t.val2 - 5000)) / 10000;

            /* Update GATT value */
            gatt_temp_centi = (int16_t)centi;

            /* Update manufacturer data in Adv (0.01 °C LE) */
            mfg_buf[2] = (uint8_t)(centi & 0xFF);
            mfg_buf[3] = (uint8_t)((centi >> 8) & 0xFF);
#if !defined(CONFIG_BT_EXT_ADV)
            (void)bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
#else
            /* For ext adv, you would call bt_le_ext_adv_set_data() on your adv handle */
#endif

            /* Notify if a client enabled notifications */
            if (temp_ntf_enabled) {
                (void)bt_gatt_notify(NULL, &env_svc.attrs[2],
                                     &gatt_temp_centi, sizeof(gatt_temp_centi));
            }

            int32_t whole = centi / 100;
            int32_t frac  = centi < 0 ? -(centi % 100) : (centi % 100);
            printk("Temp = %d.%02d C\n", whole, frac);
        } else {
            printk("DS18B20 read failed\n");
        }

        if (atomic_test_and_clear_bit(state, STATE_CONNECTED)) {
#if defined(HAS_LED)
            blink_stop();
#endif
        } else if (atomic_test_and_clear_bit(state, STATE_DISCONNECTED)) {
#if !defined(CONFIG_BT_EXT_ADV)
            printk("Restarting Legacy Advertising\n");
            (void)bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
#else
            printk("Restarting Extended Advertising\n");
            /* If using ext adv, restart your adv set here */
#endif
#if defined(HAS_LED)
            blink_start();
#endif
        }
    }
}
