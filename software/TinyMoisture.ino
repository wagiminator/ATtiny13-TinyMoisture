// TinyMoisture - Soil Moisture Monitor based on ATtiny13A
//
// The ATtiny13 spends most of its time in sleep mode. Every eight
// seconds it is woken up by the watchdog timer. It measures the
// moisture in the soil and compares it with the threshold value
// selected by the calibration potentiometer. If the soil is too dry,
// it emits a short acoustic signal via the buzzer and a short optical
// signal via the LED (ALARM). Then it goes back to sleep mode.
//
// The ATtiny can also be woken up by pressing the TEST button (pin
// change interrupt). An acoustic and optical signal is then output
// first so that the user can determine whether the battery is still
// supplying sufficient power. Then measurements of the soil moisture
// are carried out until the button is released again. In this way,
// the correct threshold value can be found and set by turning the poti.
//
// To calibrate the TinyMoisture it should be placed in soil that is
// just too dry. Press and hold the TEST button. Turn the calibration
// potentiometer until the alarm is just triggered. Release the TEST
// button.
//
//                             +-\/-+
// ------------- A0 (D5) PB5  1|    |8  Vcc
// MEASURE ----- A3 (D3) PB3  2|    |7  PB2 (D2) A1 --- TEST BUTTON
// CALIBRATION - A2 (D4) PB4  3|    |6  PB1 (D1) ------ ALARM
//                       GND  4|    |5  PB0 (D0) ------ POWER ENABLE
//                             +----+    
//
// Controller:  ATtiny13
// Core:        MicroCore (https://github.com/MCUdude/MicroCore)
// Clockspeed:  128 kHz internal
// BOD:         BOD disabled (energy saving)
// Timing:      Micros disabled
//
// 2021 by Stefan Wagner 
// Project Files (EasyEDA): https://easyeda.com/wagiminator
// Project Files (Github):  https://github.com/wagiminator
// License: http://creativecommons.org/licenses/by-sa/3.0/


// libraries
#include <avr/io.h>             // for gpio
#include <avr/sleep.h>          // for the sleep modes
#include <avr/interrupt.h>      // for the interrupts
#include <avr/power.h>          // for the power control
#include <avr/wdt.h>            // for the watch dog timer
#include <util/delay.h>         // for delays

// pin definitions
#define PE_PIN  PB0             // power enable output
#define AL_PIN  PB1             // alarm output
#define TB_PIN  PB2             // test button input
#define MS_PIN  PB3             // measurement input
#define CA_PIN  PB4             // calibration poti input

// ADC port definitions
#define MS_ADC  3               // ADC port of measurement
#define CA_ADC  2               // ADC port of calibration poti

// reset watchdog timer
void resetWatchdog(void) {
  cli();                                  // timed sequence coming up
  MCUSR = 0;                              // clear various "reset" flags
  WDTCR = (1<<WDCE)|(1<<WDE)|(1<<WDTIF);  // allow changes, disable reset, clear existing interrupt
  WDTCR = (1<<WDTIE)|(1<<WDP3)|(1<<WDP0); // set WDT interval 8 seconds
  wdt_reset();                            // pat the dog
  sei();                                  // interrupts are required now
}

// go to sleep in order to save energy, wake up again by watchdog timer or pin change interrupt
void sleep(void) { 
  GIFR |= (1<<PCIF);                      // clear any outstanding interrupts
  resetWatchdog();                        // get watchdog ready
  sleep_mode();                           // sleep
}

// analog read ADC
uint16_t readADC(uint8_t port) {
  ADMUX   = (port & 0x03);                // set port against Vcc
  ADCSRA |= (1<<ADSC);                    // start ADC conversion
  while(ADCSRA & (1<<ADSC));              // wait for ADC conversion complete
  return(ADC);                            // read and return measurement
}

// create a short beep on the buzzer
void beep(void) {
  for (uint8_t i=255; i; i--) {
    PORTB |=  (1<<AL_PIN);
    _delay_us(125);
    PORTB &= ~(1<<AL_PIN);
    _delay_us(125);
  }
}

// main function
int main(void) {
  // reset watchdog timer
  resetWatchdog();                        // do this first in case WDT fires

  // setup pins
  DDRB  = (1<<PE_PIN)|(1<<AL_PIN);        // set output pins
  PORTB = (1<<TB_PIN);                    // set pull-ups

  // disable unused peripherals and set sleep mode to save power
  ACSR   = (1<<ACD);                      // disable analog comperator
  DIDR0 |= (1<<MS_PIN)|(1<<CA_PIN);       // disable digital intput buffer on ADC pins
  PRR    = (1<<PRTIM0);                   // shut down timer0
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // set sleep mode to power down

  // setup pin change interrupt
  GIMSK = (1<<PCIE);                      // pin change interrupts enable
  PCMSK = (1<<TB_PIN);                    // set pins for pin change interrupt
  sei();                                  // enable global interrupts

  // main loop
  while(1) {
    if (~PINB & (1<<TB_PIN)) beep();      // alarm if test button is pressed

    ADCSRA |=  (1<<ADEN);                 // enable ADC
    PORTB  |=  (1<<PE_PIN);               // power measurement circuit
    _delay_us(125);                       // give it all a little time

    do {     
      if (readADC(MS_ADC) < (readADC(CA_ADC)>>1)) beep();  // beep if dry soil    
    } while(~PINB & (1<<TB_PIN));         // repeat if test button is hold

    PORTB  &= ~(1<<PE_PIN);               // power off to prevent corrosion
    ADCSRA &= ~(1<<ADEN);                 // disable ADC

    sleep();                              // sleep for a while ...
  }
}

// watchdog interrupt service routine
ISR (WDT_vect) {
  wdt_disable();                          // disable watchdog
}

// pin change interrupt service routine
EMPTY_INTERRUPT (PCINT0_vect);            // nothing to be done here
