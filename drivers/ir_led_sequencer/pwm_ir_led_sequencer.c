#define DT_DRV_COMPAT pwm_ir_led_sequencer

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>

#include <drivers/ir_led_sequencer.h>

LOG_MODULE_REGISTER(pwm_ir_led_sequencer, CONFIG_IR_LED_SEQUENCER_LOG_LEVEL);

struct pwm_sequencer_data {
    const bool* sequence_data;
    size_t sequence_len;

    size_t seq_index;

    struct k_timer timer;
	struct k_work work;
	struct k_sem semaphore; // A binary semaphore is needed here, because mutexes are reentrant/recursive

	const struct pwm_dt_spec* ir_pwm;

	uint32_t pulse;
};

struct pwm_sequencer_config {
    struct pwm_dt_spec ir_pwm;
};

static int pwm_sequencer_send_burst(const struct device* dev, const bool* sequence_data, size_t sequence_len, k_timeout_t slot_period, uint32_t period, uint32_t pulse) {
    const struct pwm_sequencer_config* config = dev->config;
    struct pwm_sequencer_data* data = dev->data;

	if (k_sem_take(&data->semaphore, K_MSEC(100)) < 0) {
		return -EBUSY;
	}

	LOG_DBG("set carrier: period = %d, pulse = %d", period, pulse);
	data->pulse = pulse;
	int ret = pwm_set_dt(&config->ir_pwm, period, 0);
	if (ret < 0) {
		return ret;
	}

    LOG_DBG("send burst with period = %d, pulse = %d, slot period = %d", data->ir_pwm->period, data->pulse, (int)slot_period.ticks);
	data->sequence_data = sequence_data;
    data->sequence_len = sequence_len;

    data->seq_index = 0;

	k_timer_start(&data->timer, K_NO_WAIT, slot_period);

    return 0;
}

static void pwm_sequencer_work_handler(struct k_work* work) {
	struct pwm_sequencer_data* data = CONTAINER_OF(work, struct pwm_sequencer_data, work);

	if (data->seq_index < data->sequence_len) {
		bool bit = data->sequence_data[data->seq_index++];

		int ret = pwm_set_pulse_dt(data->ir_pwm, bit ? data->pulse : 0);
		if (ret < 0) {
			LOG_ERR("Failed to enable PWM (%d)", ret);
			return;
		}
	} else {
		k_timer_stop(&data->timer);
		
		int ret = pwm_set_pulse_dt(data->ir_pwm, 0);
		if (ret < 0) {
			LOG_ERR("Failed to disable PWM (%d)", ret);
			return;
		}

		LOG_DBG("transmission complete");
		k_sem_give(&data->semaphore);
	}
}

static void pwm_sequencer_timer_expired(struct k_timer* timer) {
	struct pwm_sequencer_data* data = CONTAINER_OF(timer, struct pwm_sequencer_data, timer);
	k_work_submit(&data->work);
}

static const struct ir_led_sequencer_driver_api pwm_sequencer_driver_api = {
	.send_burst = pwm_sequencer_send_burst,
};

static int pwm_sequencer_init(const struct device* dev) {
    const struct pwm_sequencer_config* config = dev->config;
    struct pwm_sequencer_data* data = dev->data;

	data->ir_pwm = &config->ir_pwm;

	if (!pwm_is_ready_dt(&config->ir_pwm)) {
		LOG_ERR("LED GPIO not ready");
		return -ENODEV;
	}

	k_work_init(&data->work, &pwm_sequencer_work_handler);
	k_timer_init(&data->timer, &pwm_sequencer_timer_expired, NULL);
	k_sem_init(&data->semaphore, 1, 1);
	
    return 0;
}

#define PWM_IR_LED_SEQUENCER_INIT(inst)                                 \
    static struct pwm_sequencer_data data##inst;                        \
                                                                        \
    static const struct pwm_sequencer_config config##inst = {           \
        .ir_pwm = PWM_DT_SPEC_INST_GET(inst),                           \
    };                                                                  \
    DEVICE_DT_INST_DEFINE(inst, pwm_sequencer_init, NULL,               \
                         &data##inst, &config##inst, POST_KERNEL,       \
                         CONFIG_KERNEL_INIT_PRIORITY_DEVICE,            \
                         &pwm_sequencer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_IR_LED_SEQUENCER_INIT)
