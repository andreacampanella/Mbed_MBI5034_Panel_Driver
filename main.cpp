#include "mbed.h"
#include "Adafruit_GFX.h"
#include "mbed.h"

#define LOW 0
#define HIGH 1

#define LED_BLACK 0
#define LED_BLUE 1
#define LED_GREEN 2
#define LED_CYAN 3
#define LED_RED 4
#define LED_MAGENTA 5
#define LED_YELLOW 6
#define LED_WHITE 7

/******************************************************************************
This is an mbed code to drive 4 LED panels based on MBI5034 LED drivers ,
ported from the arduino code written by Oliver Dewdney and Jon Russell (https://github.com/JonRussell00/LEDpanels) by
Andrea Campanella (https://github.com/andreacampanella)

Tested on the F411RE , should tecnically works with any MCU with >= ram/gpio speed of the ATMega32U4 compatible with mbed

Basic Operation:

The objective was to be able to drive four panels with a single MCU. 
Each panel has two data lines, so four panels require 8 data lines, i.e. a 
single byte.


You will need at least 1.5k of ram only for the panel pixel map,
I would say than anything less then 2k is not suitable for this task.

The code has a frame buffer in RAM with 4 sets of 384 bits 
(1 bank = 64 LEDs x 2 Rows x 3 bits (RGB) = 384) for each data line. 
Four panels, two data lines each, means all four panels can be driven by a byte 
wide frame buffer, assuming 3 bit colour. This means the update can be very 
efficient.


The UpdateFrame loop iterates over the line of 384 bits of serial data from the
frame buffer and clocks them out quickly.

Ideally, we needed a contiguous port on the MCU to be attached to 
8 external data lines for the four panels. But most MCUs don’t have this. 
So, I have connected 4 data lines to the high nibble on PortB and 4 data
lines to the low nibble on PortD.

If we had a contiguous port free  we could use only one port
and the UpdateFrame loop would be 1 instruction faster ! :-) But currently I 
copy the data to both ports, ignoring the high and low nibbles I don’t need.

In this version of the code UpdateFrame is runned by a Ticker every 0.0024 ms.
UpdateFrame needs to be called 4 times to update the entire panel, as the panel
is split in to 4 rows of 128 LEDs (as 2 rows of 64).

For each half a panel (one data line) there are 8 rows of 64 LEDs, addresses in 
banks. Bank 0x00 is row 0&4,  Bank 0x01 is row 1&5, Bank 0x02 is row 2&6, Bank 
0x03 is row 3&7.

Each call updates one bank and leaves it lit until the next interrupt.

This method updates the entire frame (1024 RGB LEDs) about 100 times a second.

This sketch now inherits from the Adafruit GFX library classes. This means we
can use the graphics functions like drawCircle, drawRect, drawTriangle, plus
the various text functions and fonts.
******************************************************************************/

class LedPanel : public Adafruit_GFX
{
  public:
    LedPanel() : Adafruit_GFX(64,64) {};
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    uint16_t newColor(uint8_t red, uint8_t green, uint8_t blue);
    uint16_t getColor() { return textcolor; }
    void drawBitmapMem(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
};


LedPanel panel;

//careful, not all the pins are BusOut capable.
BusOut PORTD(D3,D2,D4,D5);
BusOut PORTB(D8,D9,D10,D11);

DigitalOut myled(LED1);
DigitalOut PIN_A0 (A4); //PF1 - A0 on all Panels
DigitalOut PIN_A1 (A1); //PF6 - A1 on all Panels
DigitalOut PIN_CLK(A3); //PF4 - CLK on all Panel
DigitalOut PIN_LAT(A2); //PF5 - LAT on all Panel
DigitalOut PIN_OE (A5); //PF0 - OE on all Panels

// bb describes which data lines drive which of the 4 panels.
// By adjusting the order of the bits in the array, you can change the panel order visually.
//uint8_t bb[8] = { 0x40, 0x80, 0x10, 0x20, 0x04, 0x08, 0x01, 0x02 };
uint8_t bb[8] = { 0x01, 0x02 , 0x40, 0x80, 0x10, 0x20, 0x04, 0x08};
uint8_t frame[4][384];
uint8_t bank = 0;

//Tickers are executing a funtion every n seconds
Ticker updater;
Ticker flipper;


void FillBuffer(uint8_t b){
  for(uint8_t x=0; x<4; x++){
    for(uint16_t y=0; y<384; y++){
      frame[x][y] = b;
    }
  }
}


void setpixel(uint8_t x, uint8_t y, uint8_t col) {
  int16_t off = (x&7) + (x & 0xf8)*6 + ((y & 4)*2);
//  int16_t off = (x&7) + (x >> 3)*48 + ((y & 4)*2);
  uint8_t row = y & 3;
  uint8_t b = bb[(y&0x3f) >> 3];
  uint8_t *p = & frame[row][off];

  for(uint8_t c = 0; c < 3;c++) {
    if ( col & 1 ) {
      	*p |= b;
    } else {
      	*p &= ~b;
    }
    col >>= 1;
    p += 16;
  }
}


void UpdateFrame() {
  
  uint8_t * f = frame[bank];
  for (uint16_t n = 0; n<384; n++) {

        PORTD = *f & 0xFF;      // We use the low nibble on PortD for Panel 1 & 2        
        PORTB = (*f++>>8) & 0xff;    // We use the high nibble on PortB for Panel 3 & 4       
        
        PIN_CLK = LOW;
        PIN_CLK = HIGH;

    }

    PIN_OE = HIGH;     // disable output
    
    if (bank & 0x01) {

      PIN_A0 = HIGH;

    } else {

      PIN_A0 = LOW;
    }

    if (bank & 0x02) {

      PIN_A1 = HIGH;

    } else {

      PIN_A1 = LOW;

    }

    PIN_LAT = HIGH;   // toggle latch
    PIN_LAT = LOW;
    PIN_OE  = LOW;     // enable output

    if (++bank>3) bank=0;
    //this will give us and idea if the data is clocking out or not.
    myled = !myled;
}

void LedPanel::drawPixel(int16_t x, int16_t y, uint16_t color) {
  setpixel(x,y,color);
}

uint16_t LedPanel::newColor(uint8_t red, uint8_t green, uint8_t blue) {
  return (blue>>7) | ((green&0x80)>>6) | ((red&0x80)>>5);
}

void LedPanel::drawBitmapMem(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t i, j, byteWidth = (w + 7) / 8;
  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(bitmap[j * byteWidth + i / 8] & (128 >> (i & 7))) {
        panel.drawPixel(x+i, y+j, color);
      }
    }
  }
}


void testTriangles() {
  int n, i, 
    cx = panel.width()  / 2 - 1,
    cy = panel.height() / 2 - 1;

  panel.fillScreen(LED_BLACK);

  n = cx > cy ? cx : cy;
  for(i=0; i<n; i+=4) {
    panel.drawTriangle(
      cx    , cy - i, // peak
      cx - i, cy + i, // bottom left
      cx + i, cy + i, // bottom right
      LED_RED);
  }
}

int main() {
    printf("Start\r\n");
    updater.attach(&UpdateFrame, 0.0024); 

    uint8_t line = 0 ;
    
    while(1) {
        for (int row = 0; row < 64; ++row)
        {
          if (row == 63 )
          {
              line ++;
              if (line == 15)
              {
                  line = 0;
                  FillBuffer(0x00);
              }
          }
		
		FillBuffer(0x00);
		//panel.drawRect(row,0, 4, 15, 4);
        printf("x :%d y: %d\r\n",row,line);
        setpixel(row,line,0x04);
        wait_ms(5);
//      setpixel(row,line,0x00);

        }

    }
}
