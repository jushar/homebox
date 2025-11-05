#include "zephyr/device.h"
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

	const struct device* audio = DEVICE_DT_GET(DT_NODELABEL(remote_control_audio));
	const struct device* projector = DEVICE_DT_GET(DT_NODELABEL(remote_control_projector));
	const struct device* screen = DEVICE_DT_GET(DT_NODELABEL(remote_control_screen));


	if (!device_is_ready(audio)) {
		LOG_ERR("Audio not ready");
		return 0;
	}

	if (!device_is_ready(projector)) {
		LOG_ERR("Projector not ready");
		return 0;
	}

	if (!device_is_ready(screen)) {
		LOG_ERR("Screen not ready");
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

		k_sleep(K_MSEC(10000));
		
		remote_control_press_button(audio, REMOTE_CONTROL_BUTTON_POWER);
		remote_control_press_button(projector, REMOTE_CONTROL_BUTTON_POWER);
		remote_control_press_button(screen, REMOTE_CONTROL_BUTTON_DOWN);

		k_sleep(K_MSEC(5000));
	}

	return 0;
}
