// TinyBling - Wheel of Fortune
//
//                             +-\/-+
//           --- A0 (D5) PB5  1|°   |8  Vcc
// NEOPIXELS --- A3 (D3) PB3  2|    |7  PB2 (D2) A1 ---
// BUTTON ------ A2 (D4) PB4  3|    |6  PB1 (D1) ------
//                       GND  4|    |5  PB0 (D0) ------
//                             +----+
//
// Controller:  ATtiny13A
// Core:        MicroCore (https://github.com/MCUdude/MicroCore)
// Clockspeed:  9.6 MHz internal
// BOD:         BOD disabled
// Timing:      Micros disabled
//
// Leave the rest on default settings. Don't forget to "Burn bootloader"!
// No Arduino core functions or libraries are used. Use the makefile if 
// you want to compile without Arduino IDE.
//
// The Neopixel implementation is based on NeoController.
// https://github.com/wagiminator/ATtiny13-NeoController
//
// 2021 by Stefan Wagner 
// Project Files (EasyEDA): https://easyeda.com/wagiminator
// Project Files (Github):  https://github.com/wagiminator
// License: http://creativecommons.org/licenses/by-sa/3.0/


// Libraries
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Pins
#define NEO_PIN       PB3     // Pin for neopixels
#define BT_PIN        PB4     // Pin for button

// -----------------------------------------------------------------------------
// Neopixel Implementation for 9.6 MHz MCU Clock and 800 kHz Pixels
// -----------------------------------------------------------------------------

// NeoPixel parameter
#define NEO_GRB                                 // type of pixel: NEO_GRB or NEO_RGB

// NeoPixel macros
#define NEO_init()    DDRB |= (1<<NEO_PIN)      // set pixel DATA pin as output

// Send a byte to the pixels string
void NEO_sendByte(uint8_t byte) {               // CLK  comment
  for(uint8_t bit=8; bit; bit--) asm volatile(  //  3   8 bits, MSB first
    "sbi  %[port], %[pin]   \n\t"               //  2   DATA HIGH
    "sbrs %[byte], 7        \n\t"               // 1-2  if "1"-bit skip next instruction
    "cbi  %[port], %[pin]   \n\t"               //  2   "0"-bit: DATA LOW after 3 cycles
    "rjmp .+0               \n\t"               //  2   delay 2 cycles
    "add  %[byte], %[byte]  \n\t"               //  1   byte <<= 1
    "cbi  %[port], %[pin]   \n\t"               //  2   "1"-bit: DATA LOW after 7 cycles
    ::
    [port]  "I"   (_SFR_IO_ADDR(PORTB)),
    [pin]   "I"   (NEO_PIN),
    [byte]  "r"   (byte)
  );
}

// Write color to a single pixel
void NEO_writeColor(uint8_t r, uint8_t g, uint8_t b) {
  #if defined (NEO_GRB)
    NEO_sendByte(g); NEO_sendByte(r); NEO_sendByte(b);
  #elif defined (NEO_RGB)
    NEO_sendByte(r); NEO_sendByte(g); NEO_sendByte(b);
  #else
    #error Wrong or missing NeoPixel type definition!
  #endif
}

// Write hue value (0..191) to a single pixel
void NEO_writeHue(uint8_t hue) {
  uint8_t phase = hue >> 6;
  uint8_t step  = (hue & 63) << 2;
  uint8_t nstep = (63 << 2) - step;
  switch (phase) {
    case 0:   NEO_writeColor(nstep,  step,     0); break;
    case 1:   NEO_writeColor(    0, nstep,  step); break;
    case 2:   NEO_writeColor( step,     0, nstep); break;
    default:  break;
  }
}

// Set a single pixel and clear the others
void NEO_setPixel(uint8_t nr, uint8_t hue) {
  cli();
  for(uint8_t i=0; i<16; i++) {
    (i==nr) ? (NEO_writeHue(hue)) : (NEO_writeColor(0,0,0));
  }
  sei();
}

// -----------------------------------------------------------------------------
// Runtime delay
// -----------------------------------------------------------------------------

// Delay milliseconds
void delayms(uint8_t time) {
  while (time--) _delay_ms(1);
}

// -----------------------------------------------------------------------------
// Main Function
// -----------------------------------------------------------------------------

int main(void) {  
  // Local variables
  uint8_t number = 0;                     // current LED number
  uint8_t speed;                          // current speed of wheel
  uint8_t hue = 0;                        // current hue

  // Disable unused peripherals and prepare sleep mode to save power
  ACSR   =  (1<<ACD);                     // disable analog comparator
  DIDR0  = ~(1<<BT_PIN) & 0x1F;           // disable digital intput buffer except button
  PRR    =  (1<<PRADC);                   // shut down ADC
  set_sleep_mode(SLEEP_MODE_IDLE);        // set sleep mode idle (timer still running)
  
  // Setup timer/counter
  TCCR0B = (1<<CS00);                     // start timer with no prescaler

  // Setup
  PORTB |= (1<<BT_PIN);                   // pullup on button pin
  GIMSK |= (1<<PCIE);                     // enable pin change interrupt
  PCMSK |= (1<<BT_PIN);                   // enable PCI on button pin
  NEO_init();                             // init Neopixels
  NEO_setPixel(number, hue);              // set start pixel

  // Loop
  while(1) {    
    sleep_mode();                         // go to sleep
    if (~PINB & (1<<BT_PIN)) {            // if button pressed:
      speed = 16 + (TCNT0 & 15);          // set start speed randomly
      while(--speed) {                    // increase speed
        if(++hue > 191) hue = 0;          // next hue value
        number = (number + 1) & 15;       // next pixel number
        NEO_setPixel(number, hue);        // set next pixel
        delayms(speed);                   // delay
      }
      while(++speed < 96) {
        if(++hue > 191) hue = 0;          // next hue value
        number = (number + 1) & 15;       // next pixel number
        NEO_setPixel(number, hue);        // set next pixel
        delayms(speed);                   // delay
      }
      while(~PINB & (1<<BT_PIN));         // wait for button released
      _delay_ms(10);                      // debounce
    }
  }
}

// Pin change interrupt service routine
EMPTY_INTERRUPT (PCINT0_vect);    // nothing to be done here, just wake up from sleep
