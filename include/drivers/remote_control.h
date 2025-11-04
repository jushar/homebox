#ifndef APP_DRIVERS_REMOTE_CONTROL_H_
#define APP_DRIVERS_REMOTE_CONTROL_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	REMOTE_CONTROL_BUTTON_POWER,
	REMOTE_CONTROL_BUTTON_UP,
	REMOTE_CONTROL_BUTTON_DOWN,
	REMOTE_CONTROL_BUTTON_CANCEL,
} RemoteControlButton;

/** @brief Remote control driver class operations */
__subsystem struct remote_control_driver_api {
	/**
	 * @brief Presses a button on the remote control
	 *
	 * @param dev Remote control device instance.
	 * @param button Button to press
	 *
	 * @retval 0 if successful.
	 * @retval -errno Other negative errno code on failure.
	 */
	int (*press_button)(const struct device *dev, RemoteControlButton button);
};

/**
 * @brief Presses a remote control button
 *
 * @param dev Remote control device instance.
 * @param button Button to press
 *
 * @retval 0 if successful.
 * @retval -errno Other negative errno code on failure.
 */
__syscall int remote_control_press_button(const struct device *dev, RemoteControlButton button);

static inline int z_impl_remote_control_press_button(const struct device *dev, RemoteControlButton button)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(remote_control, dev));

	return DEVICE_API_GET(remote_control, dev)->press_button(dev, button);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/remote_control.h>

#endif /* APP_DRIVERS_REMOTE_CONTROL_H_ */
