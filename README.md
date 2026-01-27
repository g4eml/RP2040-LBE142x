# RP2040_LBE142x

## Description

Stand alone programmer for the Leo Bodner LBE-1420 and LBE-1421 GPSDOs. 

## Features

Connects to LBE-1420 or LBE-1421 using USB. Allows the current frequencies of the unit to be changed without needing to be connected to a computer. 

100 memories to store regularly used frequencies. 

OLED display to show current frequency and memory contents. 

Memories can be changed using the 5 menu buttons. 


## Requirements

RP2040 Zero Module.  (other RP2040 modules can also be used, such as the Pico)

1.3" I2C OLED display (4 Pin type).  128 x 64 pixels single colour. Available from many sources. Try to get one with the SH1106 Controller chip.

**Warning...** These displays come from various manufacturers. There does not seem to be any standard for the pin layout. Check the VCC and GND pins match the PCB layout. 
Some versions of the display have configuration jumpers to swap the VCC and Ground Pins. Configure these so that Pin 1 is GND and Pin 2 is VCC. 

5 push button switches. (Tactile switches)

USB A PCB Mount Socket. 

2 15K resistors

## Construction

A PCB Design is included. The gerber files can be sent to any of the low cost PCB manufacturers. 

Note that the USB-A socket and the two 15K resistors are fitted on the bottom side of the PCB. The RP2040 Zero, Buttons and Display are all fitted on the top side. Make sure you fit the USB-A socket before the Display. 

Alternatively the design can easily be built on matrix board. 

This is the schematic. 

[Schematic.pdf](https://github.com/g4eml/RP2040-LBE142x/blob/main/Schematic.pdf)

Top View.
[Top View](https://github.com/g4eml/RP2040-LBE142x/blob/main/TopView.jpg)

Bottom View.
[Bottom View](https://github.com/g4eml/RP2040-LBE142x/blob/main/BottomView.jpg)

## Enclosure

A 3d printable enclosure is included in the 3D Print folder.  This consists of three parts, the main box, the endcap and the pushbuttons. Note that the holes in the main box are deliberately printed undersize. These need to be drilled out to 5mm diameter. This produces
neater holes than trying to print them directly. The display window can be cut out with a sharp knife. The pushbuttons should be left connected together, do not cut them apart.

## Programming or updating the RP2040 Zero Module (quick method)

1. Locate the latest compiled firmware file 'RP2040_LBE142x.uf2' which will be found here https://github.com/g4eml/RP2040-LBE142x/releases and save it to your desktop. 

2. Connect the Module to your PC using the USB-C port.. 

3. Hold down the BOOT button on the HMI module and briefly press the Reset button. The RP2040 should appear as a USB disk drive on your PC.

3. Copy the .uf2 file onto the USB drive. The RP2040 will recognise the file and immediately update its firmware and reboot.

## Building your own version of the firmware (longer method and not normally required)

The RP2040 is programmed using the Arduino IDE with the Earl F. Philhower, III  RP2040 core. 

#### Installing the Arduino IDE

1. Download and Install the Arduino IDE 2.3.0 from here https://www.arduino.cc/en/software

2. Open the Arduino IDE and go to File/Preferences.

3. in the dialog enter the following URL in the 'Additional Boards Manager URLs' field: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

4. Hit OK to close the Dialog.

5. Go to Tools->Board->Board Manager in the IDE.

6. Type “RP2040” in the search box.

7. Locate the entry for 'Raspberry Pi Pico/RP2040 by Earle F. Philhower, III' and click 'Install'

### Installing the required libraries

1. From the Arduino IDE select (Tools > Manage Libraries)
2. Search for 'Adafruit GFX' Scroll down to find the Adafruit GFX Library by Adafruit.
3. Click Install
4. Now search for 'Adafruit SH110X' and find the Adafruit SH110X library by Adafruit..
5. Click Install

#### Downloading the Software.

1. Download the latest released source code .zip file from https://github.com/g4eml/RP2040_LBE142x/releases

2. Save it to a convenient location and then unzip it.


#### Programming the RP2040

1. Open the Arduino IDE and click File/Open

2. Navigate to the File RP2404Synth/RP2040_LBE1421x.ino (downloaded in the previous step) and click Open.

3. Select Tools and make the following settings.

   Board: "Raspberry Pi Pico"
   
   Debug Level: "None"

   Debug Port: "Disabled"

   C++ Exceptions: "Disabled"

   Flash Size: "2Mb (no FS)"

   CPU Speed: "240MHz (Overclock)"   ........ Note the USB Host Mode only works with multiples of 120MHz for the clock. 240 MHz seems to work OK on most chips. 

   IP/Bluetooth Stack: "IPV4 Only"

   Optimise: "Small (-Os)(standard)"

   RTTI: "Disabled"

   Stack Protection: "Disabled"

   Upload Method: "Default (UF2)"

   USB Stack: "Adafruit TinyUSB"  

5. Connect the RP2040 Module to the USB port, hold down the BOOT button and briefly press the reset Button. 

6. Click Sketch/Upload.

The Sketch should compile and upload automatically to the RP2040. If the upload fails you may need to disconnect the module and then hold down the BOOT button while reconnecting. 



## Firmware description

When power is applied to the RP2040 USB-C socket the firmware will start and display "LBE-1420 Programmer  G4EML 2026"

Connect the LBE-142x to the USB A socket on the programmer.  After a few seconds it should be recognised and the display will show "LBE-1420 Connected" , the current Frequencies and the GPS Lock Status. 

To change the frequency press the Enter button. 
A new line will appear at the bottom of the display. " M00=  10.000000 MHz "  (note that on first use all memories will be initialised to 10.000000 MHz.)
The M00 part of the line will be underlined, indicating the Memory number can be changed. Do this with the Up or down buttons.
When connected to a LBE-1421 each press of the up or down button will cycle between F1 and F2 before changing the memory number. 
When you have selected the memory you require press the Enter button. The memory frequency will be transfered to the LBE-142x and the status lines at the top of the screen should show it has been accepted. 

To change the memory frequencies. 
Fist select the required memory using the Up and down bottons. 
For the LBE-1421 make sure you have also selected the correct output. 
Then press the right button to move the underline cursor to the first of the frequency digits (1000 Mhz).
Position the cursor on the required digit using the left and right buttons.
Change the digit using the Up and Down buttons. 
When you have selected the frequency you require press the Enter button. 
The frequency will be transfered to the LBE-142x and will also be saved to the RP2040 EEPROM for future use. 








