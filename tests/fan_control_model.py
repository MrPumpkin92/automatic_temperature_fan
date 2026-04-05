"""Host-side model of the MSP430 smart fan logic.

This mirrors the control behavior from main.c so we can test and simulate
without target hardware.
"""

from dataclasses import dataclass
from enum import Enum
from typing import List, Tuple


class Mode(Enum):
    AUTO = 0
    MANUAL = 1


# (adc_count, celsius) sparse calibration points used in embedded code
NTC_LUT: List[Tuple[int, int]] = [
    (350, 10),
    (396, 15),
    (455, 20),
    (489, 24),
    (512, 25),
    (541, 28),
    (568, 30),
    (595, 32),
    (620, 35),
    (668, 40),
]


def clamp_adc(adc: int) -> int:
    return max(0, min(1023, int(adc)))


def thermistor_adc_to_celsius(adc: int) -> int:
    """Linear interpolation over the lookup table points."""
    adc = clamp_adc(adc)

    if adc <= NTC_LUT[0][0]:
        return NTC_LUT[0][1]

    for i in range(1, len(NTC_LUT)):
        x0, y0 = NTC_LUT[i - 1]
        x1, y1 = NTC_LUT[i]
        if adc <= x1:
            return int(y0 + ((adc - x0) * (y1 - y0)) / (x1 - x0))

    return NTC_LUT[-1][1]


def auto_duty_from_temp_c(temp_c: int) -> int:
    if temp_c < 24:
        return 0
    if temp_c < 28:
        return 35
    if temp_c < 32:
        return 65
    return 100


def manual_duty_from_adc(adc: int) -> int:
    adc = clamp_adc(adc)
    return int((adc * 100) / 1023)


@dataclass
class SmartFanController:
    mode: Mode = Mode.AUTO
    led_on: bool = True
    debounce_ms: int = 40
    last_press_ms: int = -10_000

    def set_mode(self, mode: Mode) -> None:
        self.mode = mode
        self.led_on = mode == Mode.AUTO

    def button_falling_edge(self, now_ms: int, pin_is_low: bool = True) -> bool:
        """Returns True only when mode toggle is accepted after debounce."""
        if not pin_is_low:
            return False

        if now_ms - self.last_press_ms < self.debounce_ms:
            return False

        self.last_press_ms = now_ms
        self.set_mode(Mode.MANUAL if self.mode == Mode.AUTO else Mode.AUTO)
        return True

    def sample_and_compute_duty(self, therm_adc: int, pot_adc: int) -> int:
        if self.mode == Mode.AUTO:
            temp_c = thermistor_adc_to_celsius(therm_adc)
            return auto_duty_from_temp_c(temp_c)
        return manual_duty_from_adc(pot_adc)
