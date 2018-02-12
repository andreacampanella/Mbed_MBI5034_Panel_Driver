# Mbed_MBI5034_Panel_Driver
Drive an MBI5034 based LED panel with mbed.

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