#include "zephyr/sys/printk.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/remote_control.h>
#include <zephyr/drivers/led.h>

#include <app_version.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

int main()
{
	printk("Zephyr Example Application %s\n", APP_VERSION_STRING);

	const struct device *remote_control = DEVICE_DT_GET(DT_NODELABEL(remote_control_audio));
	if (!device_is_ready(remote_control)) {
		LOG_ERR("Remote control not ready");
		return 0;
	}

	const struct device* led = DEVICE_DT_GET(DT_NODELABEL(leds));
	if (!device_is_ready(led)) {
		LOG_ERR("LED not ready");
		return 0;
	}

	while (1) {
		led_on(led, 0);
		k_sleep(K_MSEC(500));
		led_off(led, 0);
		
		int ret = remote_control_press_button(remote_control, REMOTE_CONTROL_BUTTON_POWER);
		printk("Pressing power button: %d\n", ret);

		k_sleep(K_MSEC(500));
	}

	return 0;
}
