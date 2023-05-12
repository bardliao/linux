/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CS42L43 CODEC driver external data
 *
 * Copyright (C) 2022-2023 Cirrus Logic, Inc. and
 *                         Cirrus Logic International Semiconductor Ltd.
 */

#ifndef CS42L43_ASOC_EXT_H
#define CS42L43_ASOC_EXT_H

#define CS42L43_SYSCLK		0

#define CS42L43_SYSCLK_MCLK	0
#define CS42L43_SYSCLK_SDW	1

#define CS42L43_N_BUTTONS	6

/**
 * struct cs42l43_plug_pdata - Configuration data for tip/ring sense.
 *
 * @invert: Boolean indicating pin polarity, inverted implies open-circuit
 *          whilst the jack is inserted.
 * @pullup: Boolean indicating if the cs42l43 should apply its internal
 *          pull up to the pin.
 * @fall_db_ms: Time in milliseconds a falling edge on the pin should be
 *              debounced for. Note the falling edge is considered after
 *              the invert.
 * @rise_db_ms: Time in milliseconds a rising edge on the pin should be
 *              debounced for. Note the rising edge is considered after
 *              the invert.
 */
struct cs42l43_plug_pdata {
	bool invert;
	bool pullup;
	unsigned int fall_db_ms;
	unsigned int rise_db_ms;
};

/**
 * struct cs42l43_button_pdata - Configuration data for button detect.
 *
 * @threshold: Impedance in Ohms under which this button will be
 *             considered active.
 * @button: Value to be reported for this button taken from
 *          snd_jack_types.
 */
struct cs42l43_button_pdata {
	unsigned int threshold;
	unsigned int button;
};

/**
 * struct cs42l43_jack_pdata - Configuration data for the accessory detect.
 *
 * @tip_debounce_ms: Software debounce on tip sense triggering in milliseconds.
 * @tip: Configuration data for the tip sense pin.
 * @ring: Configuration data for the ring sense pin.
 * @use_ring_sense: Boolean indicating if the ring sense pin will be used.
 * @bias_low: Select a 1.8V micbias rather than the default 2.8V.
 * @bias_sense_ua: Current at which the bias_sense clamp will engage, 0 to disable.
 * @bias_ramp_ms: Time in milliseconds the hardware allows for the headset
 *                bias to ramp up.
 * @detect_us: Time in microseconds the type detection will be run for.
 * @enable_button_automute: Enable the hardware automuting of decimator 1
 *                          when a headset button is pressed.
 * @buttons: Array of button entries.
 */
struct cs42l43_jack_pdata {
	int tip_debounce_ms;
	struct cs42l43_plug_pdata tip;
	struct cs42l43_plug_pdata ring;
	bool use_ring_sense;

	unsigned int bias_low;
	unsigned int bias_sense_ua;
	unsigned int bias_ramp_ms;
	unsigned int detect_us;

	bool enable_button_automute;
	struct cs42l43_button_pdata buttons[CS42L43_N_BUTTONS];
};

#endif /* CS42L43_ASOC_EXT_H */
