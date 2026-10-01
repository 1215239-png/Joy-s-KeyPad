#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static struct k_work_delayable led_chase_work;
static bool is_connected = false;
static int chase_idx = 1; // 2번 LED(인덱스 1)부터 시작

static void set_single_led(int active_idx) {
    for (int i = 0; i < 4; i++) {
        if (device_is_ready(leds[i].port)) {
            gpio_pin_set_dt(&leds[i], (i == active_idx) ? 1 : 0);
        }
    }
}

static void led_chase_handler(struct k_work *work) {
    if (is_connected) {
        return;
    }
    set_single_led(chase_idx);
    chase_idx = (chase_idx >= 3) ? 1 : (chase_idx + 1);
    k_work_schedule(&led_chase_work, K_MSEC(500));
}

static void update_layer_led(void) {
    if (!is_connected) {
        return;
    }
    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer < 4) {
        set_single_led(layer);
    }
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        return;
    }
    is_connected = true;
    k_work_cancel_delayable(&led_chase_work);
    update_layer_led();
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    is_connected = false;
    chase_idx = 1;
    k_work_schedule(&led_chase_work, K_NO_WAIT);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static int layer_leds_init(void) {
    for (int i = 0; i < 4; i++) {
        if (device_is_ready(leds[i].port)) {
            gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        }
    }
    k_work_init_delayable(&led_chase_work, led_chase_handler);
    k_work_schedule(&led_chase_work, K_NO_WAIT);
    return 0;
}

static int layer_event_listener(const zmk_event_t *eh) {
    update_layer_led();
    return 0;
}

SYS_INIT(layer_leds_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
ZMK_LISTENER(layer_leds, layer_event_listener);
ZMK_SUBSCRIPTION(layer_leds, zmk_layer_state_changed);
