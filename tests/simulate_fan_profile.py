"""Simple timeline simulation for the smart fan behavior.

Run:
    python tests/simulate_fan_profile.py
"""

from fan_control_model import SmartFanController


def main() -> None:
    controller = SmartFanController()

    # 100 ms sample cadence (matches embedded design)
    timeline_ms = [i * 100 for i in range(20)]

    # Start in AUTO with rising thermistor ADC values
    therm_profile = [460, 480, 490, 505, 530, 550, 575, 600, 620, 640]
    pot_profile = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    print("t(ms)\tmode\tled\ttherm\tpot\tduty%")
    for i, t in enumerate(timeline_ms[:10]):
        duty = controller.sample_and_compute_duty(
            therm_adc=therm_profile[i],
            pot_adc=pot_profile[i],
        )
        print(f"{t}\t{controller.mode.name}\t{int(controller.led_on)}\t{therm_profile[i]}\t{pot_profile[i]}\t{duty}")

    # Simulate button press at 1000 ms to enter MANUAL
    controller.button_falling_edge(now_ms=1000, pin_is_low=True)

    # Manual dial sweep
    therm_profile2 = [620] * 10
    pot_profile2 = [0, 100, 250, 400, 512, 650, 768, 900, 1023, 1023]

    for i, t in enumerate(timeline_ms[10:]):
        duty = controller.sample_and_compute_duty(
            therm_adc=therm_profile2[i],
            pot_adc=pot_profile2[i],
        )
        print(f"{t}\t{controller.mode.name}\t{int(controller.led_on)}\t{therm_profile2[i]}\t{pot_profile2[i]}\t{duty}")


if __name__ == "__main__":
    main()
