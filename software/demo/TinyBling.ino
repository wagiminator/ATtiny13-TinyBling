// ===================================================================================
// Project:   TinyBling ... it just does bling bling
// Version:   v1.0
// Year:      2021
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// Various decorative light animations using the TinyBling's neopixels.
//
// References:
// -----------
// The Neopixel implementation is based on NeoController.
// https://github.com/wagiminator/ATtiny13-NeoController
//
// Gamma correction table is adapted from Adafruit: LED Tricks.
// https://learn.adafruit.com/led-tricks-gamma-correction
//
// The lightweight pseudo random number generator based on 
// Galois linear feedback shift register is taken from Łukasz Podkalicki.
// https://blog.podkalicki.com/attiny13-pseudo-random-numbers/
//
// Wiring:
// -------
//                              +-\/-+
//           --- RST ADC0 PB5  1|°   |8  Vcc
// NEOPIXELS ------- ADC3 PB3  2|    |7  PB2 ADC1 -------- 
//    BUTTON ------- ADC2 PB4  3|    |6  PB1 AIN1 OC0B --- 
//                        GND  4|    |5  PB0 AIN0 OC0A --- 
//                              +----+
//
// Compilation Settings:
// ---------------------
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
// Fuse settings: -U lfuse:w:0x3a:m -U hfuse:w:0xff:m


// ===================================================================================
// Libraries and Definitions
// ===================================================================================

// Libraries
#include <avr/io.h>           // for GPIO
#include <avr/wdt.h>          // for the watchdog timer
#include <avr/sleep.h>        // for sleep functions
#include <avr/pgmspace.h>     // to store data in programm memory
#include <avr/interrupt.h>    // for interrupts

// Pin definitions
#define NEO_PIN       PB3     // Pin for neopixels
#define BT_PIN        PB4     // Pin for button

// ===================================================================================
// Low-Level Neopixel Implementation for 9.6 MHz MCU Clock and 800 kHz Pixels
// ===================================================================================

// NeoPixel parameter and macros
#define NEO_GRB                                 // type of pixel: NEO_GRB or NEO_RGB
#define NEO_init()    DDRB |= (1<<NEO_PIN)      // set pixel DATA pin as output

// Gamma correction table
const uint8_t PROGMEM gamma[] = {
    0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   3,   4,   5,
    6,   7,   8,  10,  11,  13,  14,  16,  18,  20,  22,  25,  27,  30,  33,  36,
   39,  43,  47,  50,  55,  59,  63,  68,  73,  78,  83,  89,  95, 101, 107, 114,
  120, 127, 135, 142, 150, 158, 167, 175, 184, 193, 203, 213, 223, 233, 244, 255
};

// NeoPixel buffer
uint8_t NEO_hue[16];
uint8_t NEO_bright[16];

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

// Write buffer to pixels
void NEO_show(void) {
  for(uint8_t i=0; i<16; i++) {
    uint8_t phase = NEO_hue[i] >> 6;
    uint8_t step  = NEO_hue[i] & 63;
    uint8_t col   = pgm_read_byte(&gamma[step >> (6 - NEO_bright[i])]);
    uint8_t ncol  = pgm_read_byte(&gamma[(63 - step) >> (6 - NEO_bright[i])]);
    switch(phase) {
      case 0:   NEO_writeColor(ncol,  col,     0); break;
      case 1:   NEO_writeColor(    0, ncol,  col); break;
      case 2:   NEO_writeColor( col,     0, ncol); break;
      default:  break;
    }
  }
}

// ===================================================================================
// High-Level Neopixel Functions
// ===================================================================================

// Set color to a single pixel
void NEO_set(uint8_t number, uint8_t hue) {
  NEO_bright[number] = 6;
  NEO_hue[number] = hue;
}

// Clear all pixels
void NEO_clear(void) {
  for(uint8_t i=0; i<16; i++) NEO_bright[i] = 0;
}

// Fill all pixel with the same color
void NEO_fill(uint8_t hue) {
  for(uint8_t i=0; i<16; i++) NEO_hue[i] = hue;
}

// Fade in all pixels one step
void NEO_fadeIn(void) {
  for(uint8_t i=0; i<16; i++) {
    if(NEO_bright[i] < 6) NEO_bright[i]++;
  }
}

// Fade out all pixels one step
void NEO_fadeOut(void) {
  for(uint8_t i=0; i<16; i++) {
    if(NEO_bright[i]) NEO_bright[i]--;
  }
}

// Circle all pixels clockwise
void NEO_cw(void) {
  uint8_t btemp = NEO_bright[15];
  uint8_t htemp = NEO_hue[15];
  for(uint8_t i=15; i; i--) {
    NEO_bright[i] = NEO_bright[i-1];
    NEO_hue[i]    = NEO_hue[i-1];
  }
  NEO_bright[0] = btemp;
  NEO_hue[0] = htemp;
}

// Circle all pixels counter-clockwise
void NEO_ccw(void) {
  uint8_t btemp = NEO_bright[0];
  uint8_t htemp = NEO_hue[0];
  for(uint8_t i=0; i<15; i++) {
    NEO_bright[i] = NEO_bright[i+1];
    NEO_hue[i] = NEO_hue[i+1];
  }
  NEO_bright[15] = btemp;
  NEO_hue[15] = htemp;
}

// ===================================================================================
// Pseudo Random Number Generator (adapted from Łukasz Podkalicki)
// ===================================================================================

// Start state (any nonzero value will work)
uint16_t rn = 0xACE1;

// Pseudo random number generator
uint16_t prng(uint16_t maxvalue) {
  rn = (rn >> 0x01) ^ (-(rn & 0x01) & 0xB400);
  return(rn % maxvalue);
}

// ===================================================================================
// Watchdog Implementation (Sleep Timer)
// ===================================================================================

// Reset Watchdog
void resetWatchdog(void) {
  cli();                                      // timed sequence coming up
  wdt_reset();                                // reset watchdog
  MCUSR = 0;                                  // clear various "reset" flags
  WDTCR = (1<<WDCE)|(1<<WDE)|(1<<WDTIF);      // allow changes, clear interrupt
  WDTCR = (1<<WDTIE)|(1<<WDP1);               // set interval 64ms
  sei();                                      // interrupts are required now
}

// Watchdog interrupt service routine
ISR(WDT_vect) {
  wdt_disable();                              // disable watchdog
}

// ===================================================================================
// Main Function
// ===================================================================================

int main(void) {
  // Reset watchdog timer
  resetWatchdog();                            // do this first in case WDT fires
  
  // Local variables
  uint8_t state = 0;                          // animation state variable
  uint8_t counter;                            // animation state duration
  uint8_t hue1, hue2, ptr1, ptr2;             // animation parameters

  // Disable unused peripherals and prepare sleep mode to save power
  ACSR   =  (1<<ACD);                         // disable analog comparator
  DIDR0  = ~(1<<BT_PIN) & 0x1F;               // disable digital intput buffer except button
  PRR    =  (1<<PRADC)|(1<<PRTIM0);           // shut down ADC and timer0
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);        // set sleep mode to power down

  // Setup
  PORTB |= (1<<BT_PIN);                       // pullup on button pin
  NEO_init();                                 // init Neopixels

  // Loop
  while(1) {
    // Animate
    switch(state) {
      case 0:   hue1 = 0; hue2 = 96; ptr1 = 0; ptr2 = 8; counter = 1;
                break;
                
      case 1:   NEO_fadeOut(); hue1 += 4; hue2 += 4;
                if(hue1 > 191) hue1 -= 192; if(hue2 > 191) hue2 -= 192;
                ptr1 = (ptr1 + 1) & 0x0F; ptr2 = (ptr2 + 1) & 0x0F;
                NEO_set(ptr1, hue1); NEO_set(ptr2, hue2); NEO_show();
                break;
             
      case 2:   NEO_fadeOut();
                for(uint8_t i=prng(4); i; i--) NEO_set(prng(16), prng(192));
                NEO_show();
                break;

      case 3:   for(uint8_t i=0; i<16; i++) NEO_set(i, prng(192));
                counter = 1; NEO_show();
                break;

      case 4:   for(uint8_t i=0; i<16; i++) {
                  NEO_hue[i] += prng(8); 
                  if(NEO_hue[i] > 191) NEO_hue[i] -= 192;
                }
                NEO_show();
                break;
                
      case 5:   hue1 = 0;
                for(uint8_t i=0; i<16; i++, hue1+=12) NEO_set(i, hue1);
                counter = 1; NEO_show();
                break;
                
      case 6:   NEO_cw(); NEO_show();
                break;

      case 7:   hue1 += 3; if(hue1 > 191) hue1 -= 192;
                NEO_fill(hue1); NEO_show();
                break;
                
      default:  break;
    }

    if(!(--counter)) {
      counter = 76;
      if(++state > 7) state = 0;
    }
    
    resetWatchdog();
    sleep_mode();
  }
}
