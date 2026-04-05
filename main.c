#include <msp430.h>
#include <stdint.h>

/*
 * Smart temperature-controlled fan system for MSP430G2553
 *
 * Hardware mapping (MSP430G2553 LaunchPad-friendly defaults):
 *  - Thermistor divider output: P1.3 / A3 (ADC10 channel 3)
 *  - Potentiometer output:      P1.4 / A4 (ADC10 channel 4)
 *  - Enable push button:        P1.5 (active low, pull-up, interrupt-driven)
 *  - Status LED:                P1.0 (ON in Auto, OFF in Manual)
 *  - Fan PWM output:            P1.2 / TA0.1 (Timer_A CCR1)
 */

#define LED_PIN             BIT0
#define FAN_PWM_PIN         BIT2
#define THERM_ADC_PIN       BIT3
#define POT_ADC_PIN         BIT4
#define BUTTON_PIN          BIT5

#define THERM_ADC_CH        INCH_3
#define POT_ADC_CH          INCH_4

/* Clock and timing */
#define SMCLK_HZ            1000000UL
#define PWM_FREQ_HZ         1000UL
#define PWM_PERIOD_TICKS    ((SMCLK_HZ / PWM_FREQ_HZ) - 1U)   /* CCR0 */
#define SAMPLE_PERIOD_MS    100U
#define BUTTON_DEBOUNCE_MS  40U

/* Fan duty levels for automatic mode */
#define FAN_OFF_PERCENT     0U
#define FAN_LOW_PERCENT     35U
#define FAN_MED_PERCENT     65U
#define FAN_HIGH_PERCENT    100U

typedef enum {
    MODE_AUTO = 0,
    MODE_MANUAL = 1
} fan_mode_t;

typedef struct {
    uint16_t adc;
    int16_t celsius;
} ntc_lut_point_t;

/*
 * Example NTC lookup table (10k NTC, 10k divider, 3.3V):
 * ADC rises as temperature rises for this divider orientation.
 * Table uses sparse calibration points; interpolation is linear between points.
 */
static const ntc_lut_point_t ntc_lut[] = {
    {350, 10},
    {396, 15},
    {455, 20},
    {489, 24},
    {512, 25},
    {541, 28},
    {568, 30},
    {595, 32},
    {620, 35},
    {668, 40}
};

static volatile fan_mode_t g_mode = MODE_AUTO;
static volatile uint8_t g_sample_due = 0U;
static volatile uint32_t g_millis = 0UL;

static volatile uint16_t g_adc_result = 0U;
static volatile uint8_t g_adc_done = 0U;

static void clock_init_1mhz(void);
static void gpio_init(void);
static void timer0_pwm_and_tick_init(void);
static void adc10_init(void);

static void set_mode(fan_mode_t mode);
static void set_fan_duty_percent(uint8_t percent);

static uint16_t adc10_read_single(uint16_t inch_bits);
static int16_t thermistor_adc_to_celsius(uint16_t adc);

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; /* Stop watchdog */

    clock_init_1mhz();
    gpio_init();
    adc10_init();
    timer0_pwm_and_tick_init();

    set_mode(MODE_AUTO);

    __enable_interrupt();

    while (1) {
        if (g_sample_due) {
            uint16_t adc_raw;

            g_sample_due = 0U;

            if (g_mode == MODE_AUTO) {
                int16_t temp_c;

                /* Automatic mode: sample thermistor and select cooling level. */
                adc_raw = adc10_read_single(THERM_ADC_CH);
                temp_c = thermistor_adc_to_celsius(adc_raw);

                if (temp_c < 24) {
                    set_fan_duty_percent(FAN_OFF_PERCENT);
                } else if (temp_c < 28) {
                    set_fan_duty_percent(FAN_LOW_PERCENT);
                } else if (temp_c < 32) {
                    set_fan_duty_percent(FAN_MED_PERCENT);
                } else {
                    set_fan_duty_percent(FAN_HIGH_PERCENT);
                }
            } else {
                uint8_t duty_percent;

                /* Manual mode: thermistor sampling disabled; only read potentiometer. */
                adc_raw = adc10_read_single(POT_ADC_CH);
                duty_percent = (uint8_t)((adc_raw * 100UL) / 1023UL);
                set_fan_duty_percent(duty_percent);
            }
        }

        /* Sleep between events; timer/button/ADC interrupts wake as needed. */
        __bis_SR_register(LPM0_bits | GIE);
        __no_operation();
    }
}

static void clock_init_1mhz(void)
{
    /* Load factory calibration for 1MHz DCO (if available). */
    if (CALBC1_1MHZ != 0xFF) {
        DCOCTL = 0;
        BCSCTL1 = CALBC1_1MHZ;
        DCOCTL = CALDCO_1MHZ;
    }

    /* SMCLK = DCO/1, ACLK default (LFXT1/VLO depending board setup). */
    BCSCTL2 = 0;
}

static void gpio_init(void)
{
    /* LED output */
    P1DIR |= LED_PIN;
    P1OUT &= (uint8_t)~LED_PIN;

    /* PWM output on TA0.1 (P1.2) */
    P1DIR |= FAN_PWM_PIN;
    P1SEL |= FAN_PWM_PIN;
    P1SEL2 &= (uint8_t)~FAN_PWM_PIN;

    /* Analog inputs for ADC10 channels A3 and A4 */
    P1DIR &= (uint8_t)~(THERM_ADC_PIN | POT_ADC_PIN);
    ADC10AE0 |= (THERM_ADC_PIN | POT_ADC_PIN);

    /* Button input with pull-up and falling-edge interrupt (active low). */
    P1DIR &= (uint8_t)~BUTTON_PIN;
    P1REN |= BUTTON_PIN;
    P1OUT |= BUTTON_PIN;
    P1IES |= BUTTON_PIN;
    P1IFG &= (uint8_t)~BUTTON_PIN;
    P1IE |= BUTTON_PIN;
}

static void timer0_pwm_and_tick_init(void)
{
    /* PWM period and initial duty */
    TA0CCR0 = (uint16_t)PWM_PERIOD_TICKS;
    TA0CCR1 = 0U;

    /* CCR1 reset/set output mode for PWM on TA0.1 */
    TA0CCTL1 = OUTMOD_7;

    /* CCR0 interrupt: used as 1 ms system tick and 100 ms sample scheduler */
    TA0CCTL0 = CCIE;

    /* Timer_A: SMCLK, Up mode, clear TAR */
    TA0CTL = TASSEL_2 | MC_1 | TACLR;
}

static void adc10_init(void)
{
    /*
     * ADC10 setup:
     * - ADC10ON: enable ADC core
     * - SHT_2: sample-and-hold time = 16 ADC10CLK cycles
     * - ADC10IE: interrupt on conversion complete
     * - ADC10SSEL_3: ADC clock source = SMCLK
     * - ADC10DIV_3: divide clock for robust acquisition timing
     *
     * Channels used in software: A3 (thermistor) and A4 (potentiometer)
     * This supports multi-channel application needs by switching channel per mode.
     */
    ADC10CTL0 = ADC10ON | ADC10SHT_2 | ADC10IE;
    ADC10CTL1 = ADC10SSEL_3 | ADC10DIV_3;
}

static void set_mode(fan_mode_t mode)
{
    g_mode = mode;

    if (mode == MODE_AUTO) {
        P1OUT |= LED_PIN; /* LED ON in auto mode */
    } else {
        P1OUT &= (uint8_t)~LED_PIN; /* LED OFF in manual mode */
    }

    /* Force a fresh control update immediately after mode switch. */
    g_sample_due = 1U;
    __bic_SR_register_on_exit(LPM0_bits);
}

static void set_fan_duty_percent(uint8_t percent)
{
    uint16_t duty;

    if (percent >= 100U) {
        TA0CCR1 = TA0CCR0; /* effectively 100% */
        return;
    }

    duty = (uint16_t)(((uint32_t)(TA0CCR0 + 1U) * percent) / 100U);
    if (duty > TA0CCR0) {
        duty = TA0CCR0;
    }

    TA0CCR1 = duty;
}

static uint16_t adc10_read_single(uint16_t inch_bits)
{
    /* Stop conversion to safely reconfigure input channel and sequence mode. */
    ADC10CTL0 &= (uint16_t)~ENC;

    /* Single-channel single-conversion, select requested channel. */
    ADC10CTL1 &= (uint16_t)~(INCH_15 | CONSEQ_3);
    ADC10CTL1 |= (inch_bits | CONSEQ_0);

    g_adc_done = 0U;
    ADC10CTL0 |= ENC | ADC10SC;

    /* Wait in LPM0 until ADC ISR posts completion. */
    while (!g_adc_done) {
        __bis_SR_register(LPM0_bits | GIE);
        __no_operation();
    }

    return g_adc_result;
}

static int16_t thermistor_adc_to_celsius(uint16_t adc)
{
    uint16_t i;

    if (adc <= ntc_lut[0].adc) {
        return ntc_lut[0].celsius;
    }

    for (i = 1U; i < (uint16_t)(sizeof(ntc_lut) / sizeof(ntc_lut[0])); i++) {
        if (adc <= ntc_lut[i].adc) {
            int32_t x0 = ntc_lut[i - 1U].adc;
            int32_t y0 = ntc_lut[i - 1U].celsius;
            int32_t x1 = ntc_lut[i].adc;
            int32_t y1 = ntc_lut[i].celsius;

            /* Linear interpolation between surrounding calibration points. */
            return (int16_t)(y0 + ((int32_t)(adc - x0) * (y1 - y0)) / (x1 - x0));
        }
    }

    return ntc_lut[(sizeof(ntc_lut) / sizeof(ntc_lut[0])) - 1U].celsius;
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
    static uint16_t ms_div_100 = 0U;

    g_millis++;

    ms_div_100++;
    if (ms_div_100 >= SAMPLE_PERIOD_MS) {
        ms_div_100 = 0U;
        g_sample_due = 1U;

        /* Wake main only every sample period to maximize low-power time. */
        __bic_SR_register_on_exit(LPM0_bits);
    }
}

#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    static uint32_t last_press_ms = 0UL;
    uint32_t now_ms;

    if (P1IFG & BUTTON_PIN) {
        P1IFG &= (uint8_t)~BUTTON_PIN;

        now_ms = g_millis;
        if ((now_ms - last_press_ms) >= BUTTON_DEBOUNCE_MS) {
            last_press_ms = now_ms;

            if ((P1IN & BUTTON_PIN) == 0U) {
                if (g_mode == MODE_AUTO) {
                    set_mode(MODE_MANUAL);
                } else {
                    set_mode(MODE_AUTO);
                }
            }
        }
    }
}

#pragma vector = ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
    g_adc_result = ADC10MEM;
    g_adc_done = 1U;

    __bic_SR_register_on_exit(LPM0_bits);
}
