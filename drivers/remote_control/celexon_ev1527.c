#define DT_DRV_COMPAT celexon_ev1527

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include <drivers/remote_control.h>

LOG_MODULE_REGISTER(celexon_ev1527, CONFIG_REMOTE_CONTROL_LOG_LEVEL);

#define TX_PACKET_BIT_LENGTH  24U
#define PATTERN_LENGTH        4U
#define PREAMBLE_LENGTH       32U
#define BASE_TX_PERIOD        K_USEC(300)
#define DEFAULT_RETRY_COUNT   5U

#define KEY_CODE_DOWN         8U
#define KEY_CODE_UP           4U
#define KEY_CODE_STOP         16U

typedef enum {
    TX_STATE_IDLE = 0,
    TX_STATE_PREAMBLE,
    TX_STATE_DATA,
} TxState;

struct celexon_ev1527_data {
    TxState tx_state;
	uint32_t tx_data;
    uint8_t tx_bit_index; // Bit index in tx_data
    uint8_t pattern_index; // Signal pattern phase (0-3)

    uint8_t preamble_index;

    uint8_t remaining_retries;

    struct k_timer tx_timer;
	const struct gpio_dt_spec* tx_pin;
};

struct celexon_ev1527_config {
	const struct gpio_dt_spec tx_pin;
    uint32_t otp_code;
};

static int celexon_ev1527_write(const struct device* dev, uint8_t key_code, uint8_t retry_count) {
    const struct celexon_ev1527_config* config = dev->config;
    struct celexon_ev1527_data* data = dev->data;

    //                      OTP code                  Data
    data->tx_data = ((config->otp_code & 0xFFFFF) << 5) | key_code; // TODO: Something is wrong with the shift here. In theory, it should be 4 instead of 5 and the key_code should also have 4 bits
    data->tx_bit_index = 0;
    data->pattern_index = 0;

    data->preamble_index = 0;
    data->tx_state = TX_STATE_PREAMBLE;

    data->remaining_retries = retry_count;

    LOG_DBG("Write to celexon: %x", data->tx_data);
    k_timer_start(&data->tx_timer, K_NO_WAIT, BASE_TX_PERIOD);

    return 0;
}

static void celexon_ev1527_timer_expired(struct k_timer* timer) {
    struct celexon_ev1527_data* data = CONTAINER_OF(timer, struct celexon_ev1527_data, tx_timer);

    if (data->tx_state == TX_STATE_PREAMBLE) {
        if (data->preamble_index++ == 0) {
            gpio_pin_set_dt(data->tx_pin, 1);
        } else {
            gpio_pin_set_dt(data->tx_pin, 0);
        }

        if (data->preamble_index >= PREAMBLE_LENGTH) {
            data->tx_state = TX_STATE_DATA;
        }
        return;
    }

    bool active_tx_bit = data->tx_data & (1 << (TX_PACKET_BIT_LENGTH - data->tx_bit_index));
    if (active_tx_bit == 0) {
        if (data->pattern_index == 0) {
            gpio_pin_set_dt(data->tx_pin, 1);
        } else {
            gpio_pin_set_dt(data->tx_pin, 0);
        }
    } else {
        if (data->pattern_index < 3) {
            gpio_pin_set_dt(data->tx_pin, 1);
        } else {
            gpio_pin_set_dt(data->tx_pin, 0);
        }
    }
    
    if (++data->pattern_index == PATTERN_LENGTH) {
        ++data->tx_bit_index;
        data->pattern_index = 0;
    }

    if (data->tx_bit_index >= TX_PACKET_BIT_LENGTH) {
        if (data->remaining_retries > 0) {
            --data->remaining_retries;
            data->tx_bit_index = 0;
            data->preamble_index = 0;
            data->tx_state = TX_STATE_PREAMBLE;
        } else {
            data->tx_state = TX_STATE_IDLE;
            k_timer_stop(&data->tx_timer);
        }
    }
}

static int celexon_map_key_code(RemoteControlButton button, uint8_t* key_code) {
    switch (button) {
        case REMOTE_CONTROL_BUTTON_UP:
            *key_code = KEY_CODE_UP;
            break;
        case REMOTE_CONTROL_BUTTON_DOWN:
            *key_code = KEY_CODE_DOWN;
            break;
        case REMOTE_CONTROL_BUTTON_CANCEL:
            *key_code = KEY_CODE_STOP;
            break;
        default:
            return -ENOTSUP;
    }

    return 0;
}

static int celexon_ev1527_press_button(const struct device* dev, RemoteControlButton button) {
    uint8_t key_code;
    int ret = celexon_map_key_code(button, &key_code);
    if (ret < 0) {
        return ret;
    }

	return celexon_ev1527_write(dev, key_code, DEFAULT_RETRY_COUNT);
}

static const struct remote_control_driver_api celexon_ev1527_driver_api = {
	.press_button = celexon_ev1527_press_button,
};

static int celexon_ev1527_init(const struct device* dev) {
	const struct celexon_ev1527_config* config = dev->config;
	struct celexon_ev1527_data* data = dev->data;

    if (!gpio_is_ready_dt(&config->tx_pin)) {
        LOG_ERR("TX pin GPIO is not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&config->tx_pin, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("TX pin could not be configured (%d)", ret);
        return ret;
    }

    memset(data, 0, sizeof(struct celexon_ev1527_data));
    data->tx_pin = &config->tx_pin; // only data is available in the timer expiry handler

    k_timer_init(&data->tx_timer, &celexon_ev1527_timer_expired, NULL);

	return 0;
}

#define CELEXON_EV1527_INIT(inst)                                  \
    static struct celexon_ev1527_data data##inst;                  \
                                                                   \
    static const struct celexon_ev1527_config config##inst = {     \
        .tx_pin = GPIO_DT_SPEC_INST_GET(inst, tx_gpios),           \
        .otp_code = DT_INST_PROP(inst, otp_code),                  \
    };                                                             \
    DEVICE_DT_INST_DEFINE(inst, celexon_ev1527_init, NULL,         \
                         &data##inst, &config##inst, POST_KERNEL,  \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,       \
                         &celexon_ev1527_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CELEXON_EV1527_INIT)
