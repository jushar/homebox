#define DT_DRV_COMPAT remote_control_benq_th534

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <drivers/remote_control.h>
#include <drivers/ir_led_sequencer.h>

LOG_MODULE_REGISTER(remote_control_benq_th534, CONFIG_REMOTE_CONTROL_LOG_LEVEL);

// The NEC protocol uses pulse distance encoding, thus the sequences have a different length
// and each sequence slot takes 562.5 us
static const bool NEC_SEQUENCE_ENCODED_0[] = {true, false};
static const bool NEC_SEQUENCE_ENCODED_1[] = {true, false, false, false};
static const bool NEC_SEQUENCE_ENCODED_SPACE[] = {false, false, false, false, false, false, false, false};
static const bool NEC_SEQUENCE_ENCODED_AGC[] = {true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true};
static const bool NEC_SEQUENCE_ENCODED_STOP[] = {true, false};

 struct remote_control_benq_th534_data {
	bool sequence[sizeof(NEC_SEQUENCE_ENCODED_AGC) + sizeof(NEC_SEQUENCE_ENCODED_SPACE) + sizeof(NEC_SEQUENCE_ENCODED_1)*8*4 + sizeof(NEC_SEQUENCE_ENCODED_STOP)];
	size_t seq_fill_index;
};

struct remote_control_benq_th534_config {
	const struct device* ir_led_sequencer;
};

static int remote_control_write_slots_to_sequence(struct remote_control_benq_th534_data* rc_data, const bool* slots, size_t length) {
    if (rc_data->seq_fill_index + length > sizeof(rc_data->sequence)) {
		printk("Sequence buffer overflow");
		return -ENOBUFS;
    }

    memcpy(&rc_data->sequence[rc_data->seq_fill_index], slots, length);
    rc_data->seq_fill_index += length;
    // printk("Writing %x (%u)\n", slots, length);
    return 0;
}

static void remote_control_write_byte_to_sequence(struct remote_control_benq_th534_data* rc_data, uint8_t byte) {
    for (size_t i = 0; i < 8; ++i) {
        bool is_bit_set = byte & (1 << i); // LSB first

        if (is_bit_set) {
            remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_1, sizeof(NEC_SEQUENCE_ENCODED_1));
        } else {
            remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_0, sizeof(NEC_SEQUENCE_ENCODED_0));
        }
    }
}

static void remote_control_write(struct remote_control_benq_th534_data* rc_data, uint8_t addr_low, uint8_t addr_high, uint8_t cmd) {
	rc_data->seq_fill_index = 0; 

    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_AGC, sizeof(NEC_SEQUENCE_ENCODED_AGC));
    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_SPACE, sizeof(NEC_SEQUENCE_ENCODED_SPACE));
    remote_control_write_byte_to_sequence(rc_data, addr_low);
    remote_control_write_byte_to_sequence(rc_data, addr_high);
    remote_control_write_byte_to_sequence(rc_data, cmd);
    remote_control_write_byte_to_sequence(rc_data, ~cmd);
    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_STOP, sizeof(NEC_SEQUENCE_ENCODED_STOP));
}

static int remote_control_benq_th534_press_button(const struct device* dev, RemoteControlButton button)
{
	const struct remote_control_benq_th534_config* config = dev->config;
	struct remote_control_benq_th534_data* data = dev->data;
	
	if (button != REMOTE_CONTROL_BUTTON_POWER) {
		return -ENOTSUP;
	}

    LOG_DBG("benq: press button");

    // Power on
    remote_control_write(data, 0x0, 0x30, 0x4F);

    return ir_led_sequencer_send_burst(config->ir_led_sequencer, data->sequence, data->seq_fill_index, K_NSEC(562500), PWM_KHZ(38), PWM_NSEC(6575));
}

static const struct remote_control_driver_api remote_control_benq_th534_driver_api = {
	.press_button = remote_control_benq_th534_press_button,
};

static int remote_control_benq_th534_init(const struct device* dev) {
	struct remote_control_benq_th534_data* data = dev->data;

	data->seq_fill_index = 0;

	return 0;
}

#define REMOTE_CONTROL_BENQ_TH534_INIT(inst)                                 \
    static struct remote_control_benq_th534_data data##inst;                 \
                                                                             \
    static const struct remote_control_benq_th534_config config##inst = {    \
        .ir_led_sequencer = DEVICE_DT_GET(DT_INST_PHANDLE(inst, ir_led_sequencer)), \
    };                                                                       \
    DEVICE_DT_INST_DEFINE(inst, remote_control_benq_th534_init, NULL,        \
                         &data##inst, &config##inst, POST_KERNEL,            \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
                         &remote_control_benq_th534_driver_api);

DT_INST_FOREACH_STATUS_OKAY(REMOTE_CONTROL_BENQ_TH534_INIT)
