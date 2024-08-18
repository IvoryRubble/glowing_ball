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

ColorRGB getColorRGB(uint8_t h, uint8_t s, uint8_t v) {
  ColorRGB color;

  if (h < 255 / 3)
  {
    color.g = map(h, 0, 255 / 3 - 1, 0, 255);
    color.r = 255 - color.g;
    color.b = 0;
  }
  else if (h < 255 * 2 / 3)
  {
    color.r = 0;
    color.b = map(h, 255 / 3, 255 * 2 / 3 - 1, 0, 255);
    color.g = 255 - color.b;
  }
  else
  {
    color.r = map(h, 255 * 2 / 3, 255, 0, 255);
    color.g = 0;
    color.b = 255 - color.r;
  }

  color.r = map(color.r, 0, 255, 0, v);
  color.g = map(color.g, 0, 255, 0, v);
  color.b = map(color.b, 0, 255, 0, v);

  return color;
}

ColorRGB getColorRGB_customPalette(uint8_t h, uint8_t s, uint8_t v) {
  ColorRGB color;

  if (h < 255 * 1 / 6)
  {
    color.g = map(h, 0, 255 * 1 / 6 - 1, 0, 255);
    color.r = 255;
    color.b = 0;
  }
  else if (h < 255 * 2 / 6)
  {
    color.g = 255;
    color.r = map(h, 255 * 1 / 6, 255 * 2 / 6 - 1, 255, 0);
    color.b = 0;
  }
  else if (h < 255 * 3 / 6)
  {
    color.g = 255;
    color.r = 0;
    color.b = map(h, 255 * 2 / 6, 255 * 3 / 6 - 1, 0, 255);
  }
  else if (h < 255 * 4 / 6)
  {
    color.g = map(h, 255 * 3 / 6, 255 * 4 / 6 - 1, 255, 0);
    color.r = 0;
    color.b = 255;
  }
  else if (h < 255 * 5 / 6)
  {
    color.g = 0;
    color.r = map(h, 255 * 4 / 6, 255 * 5 / 6 - 1, 0, 255);
    color.b = 255;
  }
  else 
  {
    color.g = 0;
    color.r = 255;
    color.b = map(h, 255 * 5 / 6, 255, 255, 0);
  }

  color.r = map(color.r, 0, 255, 0, v);
  color.g = map(color.g, 0, 255, 0, v);
  color.b = map(color.b, 0, 255, 0, v);

  return color;
}

void NEO_writeHSV(uint8_t h, uint8_t s, uint8_t v, uint8_t w)
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (h < 255 / 3)
  {
    g = map(h, 0, 255 / 3 - 1, 0, 255);
    r = 255 - g;
    b = 0;
  }
  else if (h < 255 * 2 / 3)
  {
    r = 0;
    b = map(h, 255 / 3, 255 * 2 / 3 - 1, 0, 255);
    g = 255 - b;
  }
  else
  {
    r = map(h, 255 * 2 / 3, 255, 0, 255);
    g = 0;
    b = 255 - r;
  }

  r = map(r, 0, 255, 0, v);
  g = map(g, 0, 255, 0, v);
  b = map(b, 0, 255, 0, v);

  NEO_writeColor(r, g, b, w);
}

void NEO_writeHSV_all(uint8_t h, uint8_t s, uint8_t v, uint8_t w)
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (h < 255 / 3)
  {
    g = map(h, 0, 255 / 3 - 1, 0, 255);
    r = 255 - g;
    b = 0;
  }
  else if (h < 255 * 2 / 3)
  {
    r = 0;
    b = map(h, 255 / 3, 255 * 2 / 3 - 1, 0, 255);
    g = 255 - b;
  }
  else
  {
    r = map(h, 255 * 2 / 3, 255, 0, 255);
    g = 0;
    b = 255 - r;
  }

  r = map(r, 0, 255, 0, v);
  g = map(g, 0, 255, 0, v);
  b = map(b, 0, 255, 0, v);

  NEO_writeColor_all(r, g, b, w);
}

void NEO_writeHSV_customPallete_all(uint8_t h, uint8_t s, uint8_t v, uint8_t w)
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  if (h < 255 / 4)
  {
    r = map(h, 0, 255 / 4 - 1, 0, 255);
    g = 0;
    b = 255 - r;
  }
  else if (h < 255 * 2 / 4)
  {
    b = map(h, 255 / 4, 255 * 2 / 4 - 1, 0, 255);
    r = 255 - b;
    g = 0;
  }
  else if (h < 255 * 3 / 4)
  {
    r = 0;
    g = map(h, 255 * 2 / 4, 255 * 3 / 4 - 1, 0, 255);
    b = 255 - g;
  }
  else
  {
    b = map(h, 255 * 3 / 4, 255, 0, 255);
    r = 0;
    g = 255 - b;
  }

  r = map(r, 0, 255, 0, v);
  g = map(g, 0, 255, 0, v);
  b = map(b, 0, 255, 0, v);

  NEO_writeColor_all(r, g, b, w);
}

void startupCheckColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  NEO_writeColor_all(r, g, b, w);
  NEO_latch();
  _delay_ms(500);
}

void startupCheck()
{
  NEO_clear();
  NEO_latch();
  _delay_ms(3000);

  startupCheckColor(255, 0, 0, 0);
  startupCheckColor(0, 255, 0, 0);
  startupCheckColor(0, 0, 255, 0);
  startupCheckColor(0, 0, 0, 255);
}

unsigned long currentTime = 0;

void updateAnimationSteps()
{
  unsigned long stepDuration = 100;
  unsigned long currentState = currentTime % (stepDuration * NEO_PIXELS * 3);
  unsigned long colorState = currentState / (stepDuration * NEO_PIXELS);
  unsigned long pixelCount = currentState % (stepDuration * NEO_PIXELS) / stepDuration;

  uint8_t r = colorState == 0 ? 255 : 0;
  uint8_t r1 = colorState == 1 ? 255 : 0;
  uint8_t g = colorState == 1 ? 255 : 0;
  uint8_t g1 = colorState == 2 ? 255 : 0;
  uint8_t b = colorState == 2 ? 255 : 0;
  uint8_t b1 = colorState == 0 ? 255 : 0;

  for (uint8_t i = 0; i < NEO_PIXELS; i++)
  {
    if (i < pixelCount)
    {
      NEO_writeColor(r, g, b, 0);
    }
    else
    {
      NEO_writeColor(r1, g1, b1, 0);
    }
  }
  NEO_latch();
}

void updateAnimationHueBreath()
{
  const unsigned long rainbowDuration = 10000;
  unsigned long rainbowState = currentTime % rainbowDuration;

  const unsigned long breathDuration = 3000; // 1600;
  const unsigned long breathPauseMin = 2000; // 20;
  const unsigned long breathPauseMax = 0;
  const unsigned long breathMin = 0;
  const unsigned long breathMax = 1; // 0;
  unsigned long breathState = currentTime % (breathPauseMin * 2 + breathDuration * 2 + breathPauseMax * 2);

  uint8_t value = 0;
  if (breathState < breathPauseMin || breathState >= (breathPauseMin + breathPauseMax * 2 + breathDuration * 2))
  {
    value = breathMin;
  }
  if (breathState >= (breathPauseMin + breathDuration) && breathState < (breathPauseMin + breathDuration + breathPauseMax * 2))
  {
    value = breathMax;
  }
  if (breathState >= breathPauseMin && breathState < (breathPauseMin + breathDuration))
  {
    value = map(breathState, breathPauseMin, breathPauseMin + breathDuration, breathMin, breathMax);
  }
  if (breathState >= (breathPauseMin + breathDuration + breathPauseMax * 2) && breathState < (breathPauseMin + breathDuration * 2 + breathPauseMax * 2))
  {
    value = map(breathState, breathPauseMin + breathDuration + breathPauseMax * 2, breathPauseMin + breathDuration * 2 + breathPauseMax * 2, breathMax, breathMin);
  }

  uint8_t hue = map(rainbowState, 0, rainbowDuration, 0, 255);
  uint8_t w = value; // = 255 - map(value, breathMin, breathMax, 0, 255);
  uint8_t v = 255;   // = value;
  NEO_writeHSV_customPallete_all(hue, 0, v, w);
  NEO_latch();
}

void updateAnimationHue()
{
  unsigned long rainbowAnimationDuration = 10000;
  unsigned long rainbowState = currentTime % rainbowAnimationDuration;

  uint8_t hue = map(rainbowState, 0, rainbowAnimationDuration, 0, 255);
  NEO_writeHSV_all(hue, 0, 255, 0);
  NEO_latch();
}

void updateAnimationHue_customPalette()
{
  unsigned long rainbowAnimationDuration = 20000;
  unsigned long rainbowState = currentTime % rainbowAnimationDuration;

  uint8_t hue = map(rainbowState, 0, rainbowAnimationDuration, 0, 255);
  ColorRGB color = getColorRGB_customPalette(hue, 255, 255);
  
  NEO_writeColor_all(color.r, color.g, color.b, 0);
  NEO_latch();
}

void updateAnimationSolidColor()
{
  NEO_writeColor_all(255, 100, 0, 10);
  NEO_latch();
}

void updateAnimationHalf() 
{
  unsigned long rainbowAnimationDuration = 10000;
  unsigned long rainbowState = currentTime % rainbowAnimationDuration;
  unsigned long stepState = currentTime % (rainbowAnimationDuration * 2) / rainbowAnimationDuration;

  int pixelCount1 = NEO_PIXELS / 2;

  uint8_t hue1 = map(rainbowState, 0, rainbowAnimationDuration, 0, 255);
  ColorRGB color1 = getColorRGB_customPalette(hue1, 255, 255);

  if (stepState == 0) 
  {
    for (uint8_t i = 0; i < NEO_PIXELS; i++)
    {
      if (color1.b == 0) 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(0, color1.g, 0, 0);
        }
        else
        {
          NEO_writeColor(color1.r, 0, 0, 0);
        }
      } 
      else if (color1.r == 0) 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(0, color1.g, 0, 0);
        }
        else
        {
          NEO_writeColor(0, 0, color1.b, 0);
        }
      }
      else 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(color1.r, 0, 0, 0);
        }
        else
        {
          NEO_writeColor(0, 0, color1.b, 0);
        }
      }
    }
  }
  else
  {
    for (uint8_t i = 0; i < NEO_PIXELS; i++)
    {
      if (color1.b == 0) 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(color1.r, 0, 0, 0);
        }
        else
        {
          NEO_writeColor(0, color1.g, 0, 0);
        }
      } 
      else if (color1.r == 0) 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(0, 0, color1.b, 0);
        }
        else
        {
          NEO_writeColor(0, color1.g, 0, 0);
        }
      }
      else 
      {
        if (i < pixelCount1) 
        {
          NEO_writeColor(0, 0, color1.b, 0);
        }
        else
        {
          NEO_writeColor(color1.r, 0, 0, 0);
        }
      }
    }
  }
  NEO_latch();
}

void updateAnimationWrapper()
{
  //updateAnimationHueBreath();
  //updateAnimationHue();
  //updateAnimationHue_customPalette();
  //updateAnimationSteps();
  //updateAnimationSolidColor();
  updateAnimationHalf();

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
  //startupCheck();

  // Loop
  while (1)
  {
    updateAnimationWrapper();
  }
}
