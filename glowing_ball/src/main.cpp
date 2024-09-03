// Wiring:
// -------
//                                +-\/-+
//             --- RST ADC0 PB5  1|°   |8  Vcc
//   NEOPIXELS ------- ADC3 PB3  2|    |7  PB2 ADC1 --------
//             ------- ADC2 PB4  3|    |6  PB1 AIN1 OC0B ---
//                          GND  4|    |5  PB0 AIN0 OC0A ---
//                                +----+
//
// Compilation Settings:
// ---------------------
// Controller:  ATtiny13A
// Core:        MicroCore (https://github.com/MCUdude/MicroCore)
// Clockspeed:  9.6 MHz internal
// BOD:         BOD disabled
// Timing:      Micros disabled
//
// Fuse settings: -U lfuse:w:0x3a:m -U hfuse:w:0xff:m

// ===================================================================================
// Libraries and Definitions
// ===================================================================================

// Libraries
#include <avr/io.h>        // for GPIO
#include <avr/sleep.h>     // for sleep functions
#include <avr/pgmspace.h>  // to store data in programm memory
#include <avr/interrupt.h> // for interrupts
#include <util/delay.h>    // for delays
#include "map.h"

// Pin definitions
#define NEO_PIN PB3 // Pin for neopixels

#define NEO_PIXELS 13 // number of pixels in the string (max 255)

// ===================================================================================
// Neopixel Implementation for 9.6 MHz MCU Clock and 800 kHz Pixels
// ===================================================================================

// NeoPixel parameter and macros
#define NEO_latch() _delay_us(281)        // delay to show shifted colors

// Send a byte to the pixels string
void NEO_sendByte(uint8_t byte)
{ // CLK  comment
  for (uint8_t bit = 8; bit; bit--)
    asm volatile(                     //  3   8 bits, MSB first
        "sbi  %[port], %[pin]   \n\t" //  2   DATA HIGH
        "sbrs %[byte], 7        \n\t" // 1-2  if "1"-bit skip next instruction
        "cbi  %[port], %[pin]   \n\t" //  2   "0"-bit: DATA LOW after 3 cycles
        "rjmp .+0               \n\t" //  2   delay 2 cycles
        "add  %[byte], %[byte]  \n\t" //  1   byte <<= 1
        "cbi  %[port], %[pin]   \n\t" //  2   "1"-bit: DATA LOW after 7 cycles
        ::
            [port] "I"(_SFR_IO_ADDR(PORTB)),
        [pin] "I"(NEO_PIN),
        [byte] "r"(byte));
}

// Write color to a single pixel
void NEO_writeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  NEO_sendByte(g);
  NEO_sendByte(r);
  NEO_sendByte(b);
  NEO_sendByte(w);
}

void NEO_writeColor_all(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  for (uint8_t i = NEO_PIXELS; i; i--)
    NEO_writeColor(r, g, b, w);
}

// Switch off all pixels
void NEO_clear(void)
{
  for (uint8_t i = NEO_PIXELS * 3; i; i--)
    NEO_writeColor(0, 0, 0, 0);
}

typedef struct ColorRGB {
  int r;
  int g;
  int b;
} ColorRGB;

unsigned long currentTime = 0;

void updateAnimation() 
{
  unsigned long rainbowAnimationDuration = 30000;
  unsigned long rainbowState = currentTime % rainbowAnimationDuration;
  unsigned long colorMax = 255 * NEO_PIXELS;
  unsigned long hueMax = colorMax * 6;
  unsigned long hue = map(rainbowState, 0, rainbowAnimationDuration, 0, hueMax);
  
  unsigned long r;
  unsigned long g;
  unsigned long b;
  if (hue <= hueMax * 1 / 6)
  {
    g = map(hue, 0, hueMax * 1 / 6, 0, colorMax);
    r = colorMax;
    b = 0;
  }
  else if (hue <= hueMax * 2 / 6)
  {
    g = colorMax;
    r = map(hue, hueMax * 1 / 6, hueMax * 2 / 6, colorMax, 0);
    b = 0;
  }
  else if (hue <= hueMax * 3 / 6)
  {
    g = colorMax;
    r = 0;
    b = map(hue, hueMax * 2 / 6, hueMax * 3 / 6, 0, colorMax);
  }
  else if (hue <= hueMax * 4 / 6)
  {
    g = map(hue, hueMax * 3 / 6 + 1, hueMax * 4 / 6, colorMax, 0);
    r = 0;
    b = colorMax;
  }
  else if (hue <= hueMax * 5 / 6)
  {
    g = 0;
    r = map(hue, hueMax * 4 / 6 + 1, hueMax * 5 / 6, 0, colorMax);
    b = colorMax;
  }
  else 
  {
    g = 0;
    r = colorMax;
    b = map(hue, hueMax * 5 / 6 + 1, hueMax, colorMax, 0);
  }

  for (unsigned int i = 0; i < NEO_PIXELS; i++) 
  {
    //NEO_writeColor(i < (r / 255) ? 255 : (r % 255), i < (g / 255) ? 255 : (g % 255), i < (b / 255) ? 255 : (b % 255), 0);
    NEO_writeColor(
      i < (r / 255) ? 255 : (i == (r / 255) ? (r % 255) : 0), 
      i < (g / 255) ? 255 : (i == (g / 255) ? (g % 255) : 0), 
      i < (b / 255) ? 255 : (i == (b / 255) ? (b % 255) : 0), 
      0
    );
  }

  NEO_latch();
}

void updateAnimationWrapper()
{
  updateAnimation();

  _delay_ms(1);
  currentTime++;
}

// ===================================================================================
// Main Function
// ===================================================================================

int main(void)
{
  // Setup
  cli();
  // _delay_ms(3000);
  PORTB = 0b00111111;
  DDRB = (1 << NEO_PIN); // set pixel DATA pin as output

  // Loop
  while (1)
  {
    updateAnimationWrapper();
  }
}
