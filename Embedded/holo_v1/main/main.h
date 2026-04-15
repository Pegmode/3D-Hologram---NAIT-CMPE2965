#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------
// USER SETTINGS (edit these)
// -----------------------

// SPI pins (set these to whatever pins you wired to the shift-register chain)
#define PIN_SPI_MOSI    	39
#define PIN_SPI_SCLK    	40
#define PIN_SPI_MISO    	-1   // not used

// Shift-register control pins
#define PIN_SR_LE      		41   // latch enable (LE)
#define PIN_SR_NOT_OE      	42   // output enable (OE) - active LOW; set -1 if not connected

//led power
#define PIN_BUCK_4V_EN		9	//pull up to enable 4v buck converter
#define PIN_BUCK_4V_PG		10 	//goes high to signal buck converter is ready for use

//encoder pins
#define PIN_ENC_A			4
#define PIN_ENC_B			5
#define PIN_ENC_Z			6
#define ENC_PPR             2000

// Encoder count mode used by encoder.c.
//
// The current PCNT setup counts both edges of channel A only, which gives x2
// counting relative to the encoder's base pulses-per-revolution spec.
#define ENC_COUNT_MULTIPLIER 2
#define ENC_COUNTS_PER_REV   (ENC_PPR * ENC_COUNT_MULTIPLIER)


//pwm
#define PIN_ESC_PWM			17

// Optional debug pins 
#define PIN_SW_OPT1         21    // set -1 if not used
#define PIN_SW_OPT2			14
#define PIN_LED_RED			11
#define PIN_LED_YELLOW		12
#define PIN_LED_BLUE		13

// SPI host selection (ESP32-S3 typically uses SPI2_HOST or SPI3_HOST)
#define SR_SPI_HOST     SPI2_HOST

// SPI clock rate (start lower for clean waveforms; increase later)
#define SR_SPI_HZ       (8 * 1000 * 1000)  // 8 MHz

// Global display brightness PWM settings on OE.
#define SR_PWM_HZ       (20 * 1000)        // 20 kHz global dimming PWM
#define SR_PWM_STARTUP_BRIGHTNESS_PERCENT 10U

// Display strobe settings.
//
// Each slice can be shown for only a fraction of its angular interval by
// gating the OE PWM output. The hybrid pulse width is recomputed once per
// revolution from the measured revolution period and active step count, then
// clamped between the bounds below.
#define DISPLAY_STROBE_ENABLED       1
#define DISPLAY_STROBE_DUTY_PERCENT  8U
#define DISPLAY_STROBE_MIN_ON_US     40U
#define DISPLAY_STROBE_MAX_ON_US     120U

// Our frame is 512 LEDs -> 512 bits -> 64 bytes
#define SR_FRAME_BYTES  64
#define DISPLAY_SLICE_BYTES SR_FRAME_BYTES

// How often to send the frame in this test (later this will be encoder-triggered)
#define TEST_SEND_PERIOD_MS  1000

#ifdef __cplusplus
}
#endif

#endif // MAIN_H
