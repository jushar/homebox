#define DT_DRV_COMPAT remote_control_rc5

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <drivers/remote_control.h>

LOG_MODULE_REGISTER(remote_control_rc5, CONFIG_REMOTE_CONTROL_LOG_LEVEL);

struct remote_control_rc5_data {
	bool sequence[14*2];
	size_t seq_index;

	struct k_timer tx_timer;
	struct k_work tx_work;

	const struct pwm_dt_spec* ir_led;
};

struct remote_control_rc5_config {
	struct pwm_dt_spec ir_led;
};

static int remote_control_rc5_press_button(const struct device *dev, RemoteControlButton button)
{
	const struct remote_control_rc5_config* config = dev->config;
	struct remote_control_rc5_data* data = dev->data;
	
	int ret;

	if (button != REMOTE_CONTROL_BUTTON_POWER) {
		return -ENOTSUP;
	}

	// RC5 address and command for power button
    uint16_t addr = 0x14;
    uint16_t cmd = 0xC; // POWER
    //uint16_t cmd = 45; // CD open/close

    static uint16_t toggle = 0;
	toggle = !toggle;
	uint16_t rc5_data = (cmd & 0x3f) | ((addr & 0x1f) << 6) | ((toggle & 1) << 11) | 0x1000 | 0x2000;
    printk("rc5 data: %x\n (toggle: %u)", rc5_data, toggle);

	data->seq_index = 0;

	size_t fill_index = 0;
	for (int i = 0; i < 14; i++) {
		bool bit = rc5_data & (1 << (13 - i));
		data->sequence[fill_index++] = !bit;
		data->sequence[fill_index++] = bit;
	}

	k_timer_start(&data->tx_timer, K_NO_WAIT, K_USEC(889));

	return 0;
}

static void remote_control_tx_work_handler(struct k_work* work)
{
	struct remote_control_rc5_data* data = CONTAINER_OF(work, struct remote_control_rc5_data, tx_work);

	if (data->seq_index < 14*2) {
		bool bit = data->sequence[data->seq_index++];

		int ret = pwm_set_pulse_dt(data->ir_led, bit ? PWM_NSEC(8333) : 0);
		/*if (ret < 0) {
			return ret;
		}*/
	} else {
		k_timer_stop(&data->tx_timer);
		
		// turn off
		int ret = pwm_set_pulse_dt(data->ir_led, 0);
		/*if (ret < 0) {
			LOG_ERR("Failed to set PWM (%d)", ret);
			return ret;
		}*/
	}
}

static void remote_control_tx_timer_expired(struct k_timer* timer)
{
	struct remote_control_rc5_data* data = CONTAINER_OF(timer, struct remote_control_rc5_data, tx_timer);
	k_work_submit(&data->tx_work);
}

static const struct remote_control_driver_api remote_control_rc5_driver_api = {
	.press_button = remote_control_rc5_press_button,
};

static int remote_control_rc5_init(const struct device *dev)
{
	const struct remote_control_rc5_config* config = dev->config;
	struct remote_control_rc5_data* data = dev->data;

	data->seq_index = 0;
	data->ir_led = &config->ir_led;

	if (!pwm_is_ready_dt(&config->ir_led)) {
		LOG_ERR("LED GPIO not ready");
		return -ENODEV;
	}

	k_work_init(&data->tx_work, &remote_control_tx_work_handler);
	k_timer_init(&data->tx_timer, &remote_control_tx_timer_expired, NULL);

	return 0;
}

#define REMOTE_CONTROL_RC5_INIT(inst)                                          \
    static struct remote_control_rc5_data data##inst;                         \
                                                                             \
    static const struct remote_control_rc5_config config##inst = {           \
        .ir_led = PWM_DT_SPEC_INST_GET(inst),      \
    };                                                                       \
    DEVICE_DT_INST_DEFINE(inst, remote_control_rc5_init, NULL,              \
                         &data##inst, &config##inst, POST_KERNEL,           \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
                         &remote_control_rc5_driver_api);

DT_INST_FOREACH_STATUS_OKAY(REMOTE_CONTROL_RC5_INIT)
