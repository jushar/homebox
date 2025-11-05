#define DT_DRV_COMPAT remote_control_rc5

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <drivers/remote_control.h>
#include <drivers/ir_led_sequencer.h>

LOG_MODULE_REGISTER(remote_control_rc5, CONFIG_REMOTE_CONTROL_LOG_LEVEL);

struct remote_control_rc5_data {
	bool sequence[14*2];
};

struct remote_control_rc5_config {
	const struct device* ir_led_sequencer;
};

static int remote_control_rc5_press_button(const struct device *dev, RemoteControlButton button)
{
	const struct remote_control_rc5_config* config = dev->config;
	struct remote_control_rc5_data* data = dev->data;

	if (button != REMOTE_CONTROL_BUTTON_POWER) {
		return -ENOTSUP;
	}

    LOG_DBG("philips: press button");

	// RC5 address and command for power button
    uint16_t addr = 0x14;
    uint16_t cmd = 0xC; // POWER
    //uint16_t cmd = 45; // CD open/close

    static uint16_t toggle = 0;
	toggle = !toggle;
	uint16_t rc5_data = (cmd & 0x3f) | ((addr & 0x1f) << 6) | ((toggle & 1) << 11) | 0x1000 | 0x2000;
    LOG_DBG("rc5 data: %x\n (toggle: %u)", rc5_data, toggle);

	size_t fill_index = 0;
	for (int i = 0; i < 14; i++) {
		bool bit = rc5_data & (1 << (13 - i));
		data->sequence[fill_index++] = !bit;
		data->sequence[fill_index++] = bit;
	}
	
	return ir_led_sequencer_send_burst(config->ir_led_sequencer, data->sequence, sizeof(data->sequence), K_USEC(889), PWM_KHZ(36), PWM_NSEC(8333));
}

static const struct remote_control_driver_api remote_control_rc5_driver_api = {
	.press_button = remote_control_rc5_press_button,
};

static int remote_control_rc5_init(const struct device *dev)
{
	struct remote_control_rc5_data* data = dev->data;
	memset(data->sequence, 0, sizeof(data->sequence));

	return 0;
}

#define REMOTE_CONTROL_RC5_INIT(inst)                                          \
    static struct remote_control_rc5_data data##inst;                         \
                                                                             \
    static const struct remote_control_rc5_config config##inst = {           \
        .ir_led_sequencer = DEVICE_DT_GET(DT_INST_PHANDLE(inst, ir_led_sequencer)), \
    };                                                                       \
    DEVICE_DT_INST_DEFINE(inst, remote_control_rc5_init, NULL,              \
                         &data##inst, &config##inst, POST_KERNEL,           \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
                         &remote_control_rc5_driver_api);

DT_INST_FOREACH_STATUS_OKAY(REMOTE_CONTROL_RC5_INIT)
