# CoilWinder
A DIY electronic coil winder built using:
 - Arduino Uno
 - Pololu DRV8834 stepper driver
 - A 5V bipolar stepper motor
 - An optional cheap-o servo
 - 3D printed parts
 - SSD1306 OLED display

Required Arduino libraries:
 - Adafruit BusIO library, version 1.7.2 or later
 - Adafruit GFX library, version 1.10.6 or later
 - Adafruit SSD1306, Version 2.4.3 or later
 - StepperDriver, version 1.3.1 (by Laurentiu Badea)
 - Customized AsyncServoLib - from the libraries directory in this repository

See the documentation folder for a bill of materials and photos of a constructed winder.

See the fusion360 folder for Fusion360 CAD files and STL files for the 3D printed parts

## Installation
- Install the Arduino IDE
- Unzip this project in your ~/Arduino project directory, e.g., $HOME/Documents/Arduino/CoilWinder
- Copy AsyncServoLib from the libraries folder to your ~/Arduino/libraries directory
- Install the remaining Arduino libraries using the Arduino IDE library manager
- Open the CoilWinder.ino file in the Arduino IDE, set the board type to Arduino UNO, compile and upload
