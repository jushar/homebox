#define DT_DRV_COMPAT remote_control_benq_th534

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <drivers/remote_control.h>

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
	size_t seq_index;
    size_t seq_fill_index;

	struct k_timer tx_timer;
	struct k_work tx_work;

	const struct pwm_dt_spec* ir_led;
};

struct remote_control_benq_th534_config {
	struct pwm_dt_spec ir_led;
};

static int remote_control_write_slots_to_sequence(struct remote_control_benq_th534_data* rc_data, const bool* slots, size_t length) {
    if (rc_data->seq_fill_index + length > sizeof(rc_data->sequence)) {
		printk("Sequence buffer overflow");
		return -ENOBUFS;
    }

    memcpy(&rc_data->sequence[rc_data->seq_fill_index], slots, length);
    rc_data->seq_fill_index += length;
    printk("Writing %x (%u)\n", slots, length);
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
	rc_data->seq_index = 0;
    rc_data->seq_fill_index = 0; 

    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_AGC, sizeof(NEC_SEQUENCE_ENCODED_AGC));
    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_SPACE, sizeof(NEC_SEQUENCE_ENCODED_SPACE));
    remote_control_write_byte_to_sequence(rc_data, addr_low);
    remote_control_write_byte_to_sequence(rc_data, addr_high);
    remote_control_write_byte_to_sequence(rc_data, cmd);
    remote_control_write_byte_to_sequence(rc_data, ~cmd);
    remote_control_write_slots_to_sequence(rc_data, NEC_SEQUENCE_ENCODED_STOP, sizeof(NEC_SEQUENCE_ENCODED_STOP));
}

static int remote_control_benq_th534_press_button(const struct device *dev, RemoteControlButton button)
{
	struct remote_control_benq_th534_data* data = dev->data;
	
	int ret;

	if (button != REMOTE_CONTROL_BUTTON_POWER) {
		return -ENOTSUP;
	}

    // Power on
    remote_control_write(data, 0x0, 0x30, 0x4F);

	k_timer_start(&data->tx_timer, K_NO_WAIT, K_NSEC(562500));

	return 0;
}

static void remote_control_tx_work_handler(struct k_work* work)
{
	struct remote_control_benq_th534_data* data = CONTAINER_OF(work, struct remote_control_benq_th534_data, tx_work);

	if (data->seq_index < data->seq_fill_index) {
		bool bit = data->sequence[data->seq_index++];

		int ret = pwm_set_pulse_dt(data->ir_led, bit ? PWM_NSEC(6575) : 0); // 26.3us / 4 = 6575ns ; 26.3us / 3 = 8767ns
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
	struct remote_control_benq_th534_data* data = CONTAINER_OF(timer, struct remote_control_benq_th534_data, tx_timer);
	k_work_submit(&data->tx_work);
}

static const struct remote_control_driver_api remote_control_benq_th534_driver_api = {
	.press_button = remote_control_benq_th534_press_button,
};

static int remote_control_benq_th534_init(const struct device *dev)
{
	const struct remote_control_benq_th534_config* config = dev->config;
	struct remote_control_benq_th534_data* data = dev->data;

	data->seq_index = 0;
    data->seq_fill_index = 0;
	data->ir_led = &config->ir_led;

	if (!pwm_is_ready_dt(&config->ir_led)) {
		LOG_ERR("LED GPIO not ready");
		return -ENODEV;
	}

	k_work_init(&data->tx_work, &remote_control_tx_work_handler);
	k_timer_init(&data->tx_timer, &remote_control_tx_timer_expired, NULL);

	return 0;
}

#define REMOTE_CONTROL_BENQ_TH534_INIT(inst)                                 \
    static struct remote_control_benq_th534_data data##inst;                 \
                                                                             \
    static const struct remote_control_benq_th534_config config##inst = {    \
        .ir_led = PWM_DT_SPEC_INST_GET(inst),      \
    };                                                                       \
    DEVICE_DT_INST_DEFINE(inst, remote_control_benq_th534_init, NULL,        \
                         &data##inst, &config##inst, POST_KERNEL,            \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                 \
                         &remote_control_benq_th534_driver_api);

DT_INST_FOREACH_STATUS_OKAY(REMOTE_CONTROL_BENQ_TH534_INIT)
