# Smart Temperature Fan - Iteration History

This document records how the project evolved and why GitHub commits were delayed.

## Project Scope

Target: MSP430G2553 smart fan controller with:
- Automatic and manual control modes
- ADC10 thermistor + potentiometer inputs
- Timer_A PWM fan output
- Interrupt-driven button mode toggle with debounce
- Low-power operation using LPM0 between events

## Iteration Timeline

## Iteration 1 - Core firmware architecture (`main.c`)

What was implemented:
- Register-level initialization for clock, GPIO, ADC10, and Timer_A
- Timer_A PWM in Up mode (`CCR0` period, `CCR1` duty)
- 100 ms sample scheduling using Timer_A interrupt
- Interrupt-driven pushbutton mode toggle (active-low)
- Software debounce logic using a millisecond counter
- LPM0 sleep/wake flow for power-efficient main loop

Why this iteration mattered:
- Established a complete end-to-end control loop on embedded hardware constraints.

## Iteration 2 - Temperature conversion and control policy (`main.c`)

What was implemented:
- Thermistor ADC count to Celsius conversion via lookup table + linear interpolation
- Automatic-mode duty thresholds:
  - Below 24 C -> 0%
  - 24 C to below 28 C -> 35%
  - 28 C to below 32 C -> 65%
  - 32 C and above -> 100%
- Manual-mode mapping from potentiometer ADC (0-1023) to 0-100% duty

Why this iteration mattered:
- Converted raw sensor data into deterministic fan behavior tied to temperature bands.

## Iteration 3 - Host-side verification model (`tests/fan_control_model.py`)

What was implemented:
- Python model mirroring embedded decision logic
- Mode state machine, debounce behavior, duty mapping, and thermistor conversion

Why this iteration mattered:
- Enabled fast validation before flashing hardware.

## Iteration 4 - Automated tests (`tests/test_fan_control_model.py`)

What was implemented:
- Unit tests for:
  - Thermistor conversion behavior
  - Automatic threshold decisions
  - Manual duty mapping
  - Mode toggling and debounce
  - Correct path selection per mode

Observed result:
- All tests passed.

Why this iteration mattered:
- Added repeatable regression checks to avoid breaking logic during updates.

## Iteration 5 - Behavioral simulation (`tests/simulate_fan_profile.py`)

What was implemented:
- Timeline simulation with 100 ms cadence
- Demonstrates Auto behavior under rising temperature, then Manual behavior after button toggle
- Prints mode, LED state, inputs, and duty output

Why this iteration mattered:
- Produced a simple, readable trace of expected runtime behavior.

## Iteration 6 - Documentation and repo hygiene (`README.md`, `.gitignore`)

What was implemented:
- README expanded with section-by-section explanation of firmware
- Added run instructions for tests and simulation
- Added `.gitignore` for Python artifacts and embedded build outputs

Why this iteration mattered:
- Improved project handoff quality and reduced noisy/unnecessary tracked files.

## Why This Was Not Committed to GitHub Immediately

Practical reasons for delayed commits:
- The initial focus was getting a working embedded control loop before creating granular commit boundaries.
- Multiple files were drafted and refined quickly in one development session, then validated with tests.
- Git hygiene (`.gitignore`) and structured documentation were added after core functionality was verified.
- The intent was to avoid committing partial or unstable intermediate states.

Professional note:
- Going forward, the better workflow is to commit per iteration (firmware core, tests, simulation, docs) to preserve clear history and traceability.

## Suggested Commit Breakdown (Recommended)

1. `feat(msp430): implement register-level smart fan firmware`
2. `test(model): add host-side control model and unit tests`
3. `feat(sim): add fan behavior timeline simulator`
4. `docs: expand README with architecture and usage`
5. `chore: add .gitignore for python and embedded build artifacts`
6. `docs: add iteration history report`

## Appendix A - Code Artifacts (Separate Files)

Primary firmware:
- `main.c`

Verification and simulation:
- `tests/fan_control_model.py`
- `tests/test_fan_control_model.py`
- `tests/simulate_fan_profile.py`

Documentation:
- `README.md`
- `.gitignore`
- `ITERATION_HISTORY.md`

## Appendix B - Optional: Inline Snapshot Excerpt

You may include selected snippets of `main.c` directly in reports if required by your instructor/reviewer, but keep the canonical source as the separate file in this repository.
