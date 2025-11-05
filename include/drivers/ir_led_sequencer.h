#ifndef APP_DRIVERS_IR_LED_SEQUENCER_H_
#define APP_DRIVERS_IR_LED_SEQUENCER_H_

#include <zephyr/sys/clock.h>
#include <zephyr/device.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Remote control driver class operations */
__subsystem struct ir_led_sequencer_driver_api {
	/**
	 * @brief Presses a button on the remote control
	 *
	 * @param dev Remote control device instance.
	 * @param sequence_data On/off sequence of PWM slots
	 * @param sequence_len Number of slots in the sequence
	 * @param slot_period Period of one slot (duration how long PWM is enabled for)
	 * @param period PWM period
	 * @param pulse PWM pulse width (defining the duty cycle)
	 *
	 * @retval 0 if successful.
	 * @retval -errno Other negative errno code on failure.
	 */
	int (*send_burst)(const struct device* dev, const bool* sequence_data, size_t sequence_len, k_timeout_t slot_period, uint32_t period, uint32_t pulse);
};

/**
 * @brief Presses a button on the remote control
 *
 * @param dev Remote control device instance.
 * @param sequence_data On/off sequence of PWM slots
 * @param sequence_len Number of slots in the sequence
 * @param slot_period Period of one slot (duration how long PWM is enabled for)
 * @param period PWM period
 * @param pulse PWM pulse width (defining the duty cycle)
 *
 * @retval 0 if successful.
 * @retval -errno Other negative errno code on failure.
 */
__syscall int ir_led_sequencer_send_burst(const struct device* dev, const bool* sequence_data, size_t sequence_len, k_timeout_t slot_period, uint32_t period, uint32_t puls);

static inline int z_impl_ir_led_sequencer_send_burst(const struct device* dev,  const bool* sequence_data, size_t sequence_len, k_timeout_t slot_period, uint32_t period, uint32_t pulse) {
	__ASSERT_NO_MSG(DEVICE_API_IS(ir_led_sequencer, dev));

	return DEVICE_API_GET(ir_led_sequencer, dev)->send_burst(dev, sequence_data, sequence_len, slot_period, period, pulse);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/ir_led_sequencer.h>

#endif /* APP_DRIVERS_IR_LED_SEQUENCER_H_ */
