# automatic_temperature_fan

Smart temperature-controlled fan firmware for MSP430G2553, with Auto and Manual operating modes, interrupt-driven control, and low-power operation.

## Project Files

- `main.c` - Embedded firmware for MSP430G2553 (register-level implementation)
- `tests/fan_control_model.py` - Host-side model of the embedded control logic
- `tests/test_fan_control_model.py` - Unit tests for behavior validation
- `tests/simulate_fan_profile.py` - Simple simulation script for timeline output

## Hardware Mapping (as implemented)

- MCU: MSP430G2553
- Thermistor (ADC): P1.3 / A3
- Potentiometer (ADC): P1.4 / A4
- Enable button (active low): P1.5 with pull-up and GPIO interrupt
- Status LED: P1.0
- Fan PWM output: P1.2 / TA0.1 (Timer_A CCR1)

## How Each Section of `main.c` Works

### 1) Includes, constants, and data types

- Imports MSP430 device headers and integer types.
- Defines pin masks, ADC channel selections, timing constants, PWM parameters, and duty constants.
- Defines `fan_mode_t` for `MODE_AUTO` and `MODE_MANUAL`.
- Defines `ntc_lut_point_t` and an NTC lookup table (`ntc_lut`) used to convert ADC count to Celsius.

### 2) Global state

- `g_mode`: current mode (auto/manual).
- `g_sample_due`: set by timer every ~100 ms to request a control update.
- `g_millis`: software millisecond counter used for debounce timing.
- `g_adc_result` and `g_adc_done`: handshake variables between ADC ISR and main code.

### 3) `main()` startup flow

- Stops watchdog (`WDTCTL = WDTPW | WDTHOLD`).
- Initializes clock, GPIO, ADC10, and Timer_A.
- Starts in automatic mode (`set_mode(MODE_AUTO)`).
- Enables global interrupts.
- Main loop:
	- If `g_sample_due` is set, runs one control cycle.
	- Auto mode: reads thermistor ADC, converts to temperature, applies threshold-based duty.
	- Manual mode: reads potentiometer ADC and linearly maps ADC 0-1023 to 0-100% duty.
	- Enters LPM0 between events to reduce power.

### 4) Clock configuration: `clock_init_1mhz()`

- Loads factory 1 MHz DCO calibration (when available).
- Sets clock divider registers so SMCLK is sourced from DCO.

### 5) GPIO configuration: `gpio_init()`

- LED pin configured as output.
- PWM pin configured for peripheral output (TA0.1).
- Thermistor and potentiometer pins configured as analog inputs via `ADC10AE0`.
- Button pin configured as input with pull-up (`P1REN` + `P1OUT`) and falling-edge interrupt (`P1IES`, `P1IE`).

### 6) Timer setup: `timer0_pwm_and_tick_init()`

- `TA0CCR0` sets PWM period (Up mode top).
- `TA0CCR1` sets PWM duty.
- `TA0CCTL1 = OUTMOD_7` enables reset/set PWM output on TA0.1.
- `TA0CCTL0 = CCIE` enables CCR0 interrupt.
- `TA0CTL = TASSEL_2 | MC_1 | TACLR` starts Timer_A from SMCLK in Up mode.

### 7) ADC10 setup: `adc10_init()`

- Enables ADC core (`ADC10ON`).
- Configures sample/hold timing and interrupt-on-complete (`ADC10IE`).
- Uses SMCLK with divider for conversion timing.
- Channel selection is done dynamically in `adc10_read_single()` for A3/A4.

### 8) Mode and duty helpers

- `set_mode()`:
	- Updates mode state.
	- LED ON in Auto, OFF in Manual.
	- Forces an immediate fresh control update.
- `set_fan_duty_percent()`:
	- Converts percentage to CCR1 ticks.
	- Handles 100% as `CCR1 = CCR0`.

### 9) ADC single-conversion helper

- `adc10_read_single(inch_bits)`:
	- Clears `ENC` to reconfigure channel safely.
	- Selects one channel and single-conversion mode.
	- Starts conversion and sleeps in LPM0 until ADC ISR marks done.
	- Returns 10-bit result from `ADC10MEM`.

### 10) Thermistor conversion

- `thermistor_adc_to_celsius()`:
	- Uses LUT boundary checks.
	- Performs linear interpolation between nearest LUT points.
	- Returns approximate Celsius integer value.

### 11) Interrupt service routines

- `TIMER0_A0_ISR`:
	- Runs every timer period (configured to 1 ms with current constants).
	- Increments `g_millis`.
	- Every 100 ticks sets `g_sample_due = 1` and wakes main from LPM0.
- `PORT1_ISR`:
	- Handles button interrupt (active low).
	- Applies software debounce using `g_millis` and `BUTTON_DEBOUNCE_MS`.
	- Toggles between Auto and Manual modes.
- `ADC10_ISR`:
	- Copies ADC result from `ADC10MEM`.
	- Sets completion flag and wakes CPU.

## Mode Behavior Summary

### Automatic mode (LED ON)

- Thermistor sampled each ~100 ms.
- Temperature-to-duty thresholds:
	- `T < 24 C` -> `0%`
	- `24 C <= T < 28 C` -> `35%`
	- `28 C <= T < 32 C` -> `65%`
	- `T >= 32 C` -> `100%`

### Manual mode (LED OFF)

- Thermistor control path is bypassed.
- Potentiometer sampled each ~100 ms.
- ADC value linearly mapped to fan duty from 0% to 100%.

## Run Host-Side Tests (No Hardware Needed)

Requirements:

- Python 3.8+

Run unit tests:

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

What tests validate:

- Thermistor lookup/interpolation behavior
- Auto threshold duty transitions
- Manual ADC-to-duty mapping
- Button mode toggle and debounce behavior
- Correct control path selection per mode

## Run Simulation

Run:

```powershell
python tests/simulate_fan_profile.py
```

This prints a simple timeline showing:

- Sample time in ms
- Current mode
- LED state
- Thermistor ADC input
- Potentiometer ADC input
- Output fan duty percent

## Notes

- The NTC LUT is an example calibration and may need tuning for your specific thermistor and resistor values.
- For real hardware validation, compare measured temperature against LUT conversion and adjust points as needed.