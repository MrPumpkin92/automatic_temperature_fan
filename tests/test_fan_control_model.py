import unittest

from fan_control_model import (
    Mode,
    SmartFanController,
    auto_duty_from_temp_c,
    manual_duty_from_adc,
    thermistor_adc_to_celsius,
)


class TestThermistorConversion(unittest.TestCase):
    def test_table_endpoints(self):
        self.assertEqual(thermistor_adc_to_celsius(350), 10)
        self.assertEqual(thermistor_adc_to_celsius(668), 40)

    def test_interpolation_midpoint(self):
        # Between (489, 24) and (512, 25)
        self.assertEqual(thermistor_adc_to_celsius(500), 24)


class TestAutomaticThresholds(unittest.TestCase):
    def test_auto_duty_bands(self):
        self.assertEqual(auto_duty_from_temp_c(23), 0)
        self.assertEqual(auto_duty_from_temp_c(24), 35)
        self.assertEqual(auto_duty_from_temp_c(27), 35)
        self.assertEqual(auto_duty_from_temp_c(28), 65)
        self.assertEqual(auto_duty_from_temp_c(31), 65)
        self.assertEqual(auto_duty_from_temp_c(32), 100)


class TestManualMapping(unittest.TestCase):
    def test_manual_adc_to_percent(self):
        self.assertEqual(manual_duty_from_adc(0), 0)
        self.assertEqual(manual_duty_from_adc(1023), 100)
        self.assertEqual(manual_duty_from_adc(512), 50)


class TestModeAndDebounce(unittest.TestCase):
    def test_default_state(self):
        c = SmartFanController()
        self.assertEqual(c.mode, Mode.AUTO)
        self.assertTrue(c.led_on)

    def test_button_toggle_with_debounce(self):
        c = SmartFanController()

        toggled = c.button_falling_edge(now_ms=100, pin_is_low=True)
        self.assertTrue(toggled)
        self.assertEqual(c.mode, Mode.MANUAL)
        self.assertFalse(c.led_on)

        # Within debounce window: ignored
        toggled = c.button_falling_edge(now_ms=120, pin_is_low=True)
        self.assertFalse(toggled)
        self.assertEqual(c.mode, Mode.MANUAL)

        toggled = c.button_falling_edge(now_ms=150, pin_is_low=True)
        self.assertTrue(toggled)
        self.assertEqual(c.mode, Mode.AUTO)
        self.assertTrue(c.led_on)

    def test_sampling_path_per_mode(self):
        c = SmartFanController()

        # AUTO uses thermistor only
        duty_auto = c.sample_and_compute_duty(therm_adc=620, pot_adc=0)
        self.assertEqual(duty_auto, 100)

        c.set_mode(Mode.MANUAL)
        duty_manual = c.sample_and_compute_duty(therm_adc=350, pot_adc=256)
        self.assertEqual(duty_manual, 25)


if __name__ == "__main__":
    unittest.main()
