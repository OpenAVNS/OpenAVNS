#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usb_device.h>

/* ------------------------------------------------------------------ */
/* Square-wave generator                                               */
/* ------------------------------------------------------------------ */
/*
 * A dedicated thread toggles a GPIO pin at a user-specified half-period.
 * Thread priority is K_PRIO_PREEMPT(15), one step below the shell default
 * (K_PRIO_PREEMPT(14)), so the shell can always preempt it and "sq stop"
 * is processed promptly.
 *
 * Timing strategy:
 *   half_period < 1000 us  -> k_busy_wait()  (CPU spin, ~1 us accuracy)
 *   half_period >= 1000 us -> k_usleep()     (yields CPU, shell stays snappy)
 *
 * Commands:
 *   sq start <gpio_dev> <pin> <half_period_us>
 *   sq stop
 *   sq status
 */

static atomic_t          sq_running = ATOMIC_INIT(0);
static volatile uint32_t sq_half_us;
static const struct device *sq_dev;
static volatile gpio_pin_t  sq_pin;

#define SQ_STACK_SIZE 1024
#define SQ_PRIORITY   K_PRIO_PREEMPT(15)

static void sq_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (1) {
		if (!atomic_get(&sq_running)) {
			k_sleep(K_MSEC(10));
			continue;
		}

		gpio_pin_toggle(sq_dev, sq_pin);

		k_busy_wait(sq_half_us);
	}
}

K_THREAD_DEFINE(sq_tid, SQ_STACK_SIZE, sq_thread_fn,
		NULL, NULL, NULL, SQ_PRIORITY, 0, 0);

static int cmd_sq_start(const struct shell *sh, size_t argc, char **argv)
{
	const struct device *dev = shell_device_get_binding(argv[1]);

	if (!dev) {
		shell_error(sh, "Unknown device '%s'", argv[1]);
		return -ENODEV;
	}

	gpio_pin_t pin      = (gpio_pin_t)atoi(argv[2]);
	uint32_t   half_us  = (uint32_t)atoi(argv[3]);

	if (half_us == 0U) {
		shell_error(sh, "half-period must be >= 1 us");
		return -EINVAL;
	}

	int ret = gpio_pin_configure(dev, pin, GPIO_OUTPUT_LOW);

	if (ret < 0) {
		shell_error(sh, "gpio_pin_configure failed: %d", ret);
		return ret;
	}

	/* Write params before raising the running flag */
	sq_dev     = dev;
	sq_pin     = pin;
	sq_half_us = half_us;
	atomic_set(&sq_running, 1);

	shell_print(sh, "Started: %s pin %u  half-period %u us  (~%u Hz)",
		    argv[1], pin, half_us, 500000U / half_us);
	return 0;
}

static int cmd_sq_stop(const struct shell *sh, size_t argc, char **argv)
{
	atomic_set(&sq_running, 0);
	if (sq_dev) {
		gpio_pin_set(sq_dev, sq_pin, 0);
	}
	shell_print(sh, "Stopped");
	return 0;
}

static int cmd_sq_status(const struct shell *sh, size_t argc, char **argv)
{
	if (!atomic_get(&sq_running) || !sq_dev) {
		shell_print(sh, "Stopped");
		return 0;
	}
	uint32_t half = sq_half_us;

	shell_print(sh, "Running: %s pin %u  half-period %u us  (~%u Hz)",
		    sq_dev->name, sq_pin, half, 500000U / half);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sq_cmds,
	SHELL_CMD_ARG(start, NULL,
		"<gpio_dev> <pin> <half_period_us>\n"
		"  Toggle a pin symmetrically. Range: 1 us to 100000000 us.\n"
		"  Examples:\n"
		"    sq start gpio0 15 1        (~500 kHz, 1 us half-period)\n"
		"    sq start gpio0 15 500      (~1 kHz,   500 us half-period)\n"
		"    sq start gpio0 15 1000     (~500 Hz,  1 ms half-period)\n"
		"    sq start gpio0 15 100000   (~5 Hz,    100 ms half-period)",
		cmd_sq_start, 4, 0),
	SHELL_CMD_ARG(stop,   NULL, "Stop toggling and drive pin LOW",
		cmd_sq_stop,   1, 0),
	SHELL_CMD_ARG(status, NULL, "Show current generator state",
		cmd_sq_status, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sq, &sq_cmds, "Square-wave generator on any GPIO pin", NULL);

/* ------------------------------------------------------------------ */
/* Stim command                                                        */
/* ------------------------------------------------------------------ */
/*
 * Sets a channel's MCP4725 DAC to the requested voltage, then toggles
 * the channel's two GPIO pins in strict antiphase (one HIGH while the
 * other is LOW) at the requested frequency.
 *
 * gpio_port_set_masked_raw() writes both pins in a single register
 * access so they switch simultaneously.
 *
 * Commands:
 *   stim start <channel 1|2> <voltage_mv> <freq_hz>
 *   stim stop
 *
 * Channel pin mapping:
 *   ch1 — DAC 0x62, P0.15 (+), P0.16 (-)
 *   ch2 — DAC 0x63, P0.17 (+), P0.19 (-)
 */

static const struct device *const stim_i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static const struct device *const stim_gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static const uint8_t   stim_dac_addr[2]  = { 0x62, 0x63 };
static const gpio_pin_t stim_pin_pos[2]  = { 15, 17 };
static const gpio_pin_t stim_pin_neg[2]  = { 16, 19 };

static atomic_t          stim_running = ATOMIC_INIT(0);
static volatile uint32_t stim_half_us;
static volatile gpio_pin_t stim_pos;
static volatile gpio_pin_t stim_neg;

#define STIM_STACK_SIZE 1024
#define STIM_PRIORITY   K_PRIO_PREEMPT(15)

static void stim_set_dac(uint8_t addr, uint16_t d)
{
	uint8_t buf[2] = { (d >> 8) & 0x0F, d & 0xFF };

	i2c_write(stim_i2c, buf, sizeof(buf), addr);
}

static void stim_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (1) {
		if (!atomic_get(&stim_running)) {
			k_sleep(K_MSEC(10));
			continue;
		}

		gpio_pin_t pos = stim_pos;
		gpio_pin_t neg = stim_neg;
		gpio_port_pins_t mask = BIT(pos) | BIT(neg);
		uint32_t half = stim_half_us;

		/* Phase A: pos HIGH, neg LOW */
		gpio_port_set_masked_raw(stim_gpio, mask, BIT(pos));
		k_busy_wait(half);

		if (!atomic_get(&stim_running)) {
			gpio_port_set_masked_raw(stim_gpio, mask, 0);
			continue;
		}

		/* Phase B: pos LOW, neg HIGH */
		gpio_port_set_masked_raw(stim_gpio, mask, BIT(neg));
		k_busy_wait(half);
	}
}

K_THREAD_DEFINE(stim_tid, STIM_STACK_SIZE, stim_thread_fn,
		NULL, NULL, NULL, STIM_PRIORITY, 0, 0);

static int cmd_stim_start(const struct shell *sh, size_t argc, char **argv)
{
	int ch = atoi(argv[1]);

	if (ch < 1 || ch > 2) {
		shell_error(sh, "channel must be 1 or 2");
		return -EINVAL;
	}

	uint32_t voltage_mv = (uint32_t)atoi(argv[2]);

	if (voltage_mv > 3300U) {
		shell_error(sh, "voltage_mv must be 0-3300");
		return -EINVAL;
	}

	uint32_t freq_hz = (uint32_t)atoi(argv[3]);

	if (freq_hz == 0U) {
		shell_error(sh, "freq_hz must be >= 1");
		return -EINVAL;
	}

	uint32_t half_us = 500000U / freq_hz;

	if (half_us == 0U) {
		shell_error(sh, "frequency too high");
		return -EINVAL;
	}

	int idx = ch - 1;

	/* Stop any running stim before reconfiguring */
	atomic_set(&stim_running, 0);
	k_sleep(K_MSEC(20));

	/* Enable HV rail before configuring outputs */
	gpio_pin_configure(stim_gpio, 23, GPIO_OUTPUT_HIGH);

	/* Configure pins as outputs, initially LOW */
	gpio_pin_configure(stim_gpio, stim_pin_pos[idx], GPIO_OUTPUT_LOW);
	gpio_pin_configure(stim_gpio, stim_pin_neg[idx], GPIO_OUTPUT_LOW);

	/* Set DAC */
	uint16_t dac_val = (uint16_t)(voltage_mv * 4096U / 3300U);

	stim_set_dac(stim_dac_addr[idx], dac_val);

	/* Publish params then start thread */
	stim_pos     = stim_pin_pos[idx];
	stim_neg     = stim_pin_neg[idx];
	stim_half_us = half_us;
	atomic_set(&stim_running, 1);

	shell_print(sh, "Stim ch%d: %u mV  %u Hz  (half-period %u us)",
		    ch, voltage_mv, freq_hz, half_us);
	return 0;
}

static int cmd_stim_stop(const struct shell *sh, size_t argc, char **argv)
{
	atomic_set(&stim_running, 0);

	/* Drive all channel pins LOW */
	for (int i = 0; i < 2; i++) {
		gpio_port_set_masked_raw(stim_gpio,
					 BIT(stim_pin_pos[i]) | BIT(stim_pin_neg[i]),
					 0);
	}

	/* Zero both DACs */
	stim_set_dac(0x62, 0);
	stim_set_dac(0x63, 0);

	/* Disable HV rail last */
	gpio_pin_set(stim_gpio, 23, 0);

	shell_print(sh, "Stopped. Pins LOW, DACs zeroed, HV disabled.");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(stim_cmds,
	SHELL_CMD_ARG(start, NULL,
		"<channel 1|2> <voltage_mv> <freq_hz>\n"
		"  Set DAC and toggle pos/neg pins in antiphase at freq_hz.\n"
		"  ch1: DAC 0x62, P0.15(+) P0.16(-)    ch2: DAC 0x63, P0.17(+) P0.19(-)\n"
		"  Examples:\n"
		"    stim start 1 1000 10    (ch1, 1.0V, 10 Hz)\n"
		"    stim start 2 2500 100   (ch2, 2.5V, 100 Hz)",
		cmd_stim_start, 4, 0),
	SHELL_CMD_ARG(stop, NULL,
		"Stop stimulation, drive all pins LOW, zero both DACs",
		cmd_stim_stop, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(stim, &stim_cmds, "Stimulation output control", NULL);

/* ------------------------------------------------------------------ */
/* BLE advertisement                                                   */
/* ------------------------------------------------------------------ */
/*
 * Non-connectable advertisement for antenna testing.
 * Open nRF Connect → Scanner and look for CONFIG_BT_DEVICE_NAME.
 * RSSI at ~1m should be roughly −50 to −70 dBm on a healthy antenna.
 *
 * Commands:
 *   ble start
 *   ble stop
 */

static const struct bt_data ble_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR | BT_LE_AD_GENERAL),
};

static const struct bt_data ble_sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static bool ble_initialized;

static int cmd_ble_start(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	if (!ble_initialized) {
		err = bt_enable(NULL);
		if (err) {
			shell_error(sh, "bt_enable failed: %d", err);
			return err;
		}
		ble_initialized = true;
	}

	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY,
			      ble_ad, ARRAY_SIZE(ble_ad),
			      ble_sd, ARRAY_SIZE(ble_sd));
	if (err && err != -EALREADY) {
		shell_error(sh, "bt_le_adv_start failed: %d", err);
		return err;
	}

	shell_print(sh, "Advertising as \"%s\" — open nRF Connect to see RSSI",
		    CONFIG_BT_DEVICE_NAME);
	return 0;
}

static int cmd_ble_stop(const struct shell *sh, size_t argc, char **argv)
{
	bt_le_adv_stop();
	shell_print(sh, "Advertising stopped");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(ble_cmds,
	SHELL_CMD_ARG(start, NULL, "Start BLE advertisement", cmd_ble_start, 1, 0),
	SHELL_CMD_ARG(stop,  NULL, "Stop BLE advertisement",  cmd_ble_stop,  1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ble, &ble_cmds, "BLE advertisement for antenna testing", NULL);

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
	usb_enable(NULL);
	printk("nRF52840 bring-up shell ready\n");
	printk("Type 'help' for commands\n");
	return 0;
}

