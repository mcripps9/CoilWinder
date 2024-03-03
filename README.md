# CoilWinder
A DIY electronic coil winder built using:
 - Arduino Uno
 - Pololu DRV8834 stepper driver
 - A 5V bipolar stepper motor
 - An optional cheap-o servo
 - 3D printed parts
 - SSD1306 OLED display

Required Arduino libraries:
 - SSD1306Ascii, version 1.3.5 (by Bill Greiman)
 - StepperDriver, version 1.4.1 (by Laurentiu Badea)

See the documentation folder for a bill of materials and photos of a constructed winder.

See the fusion360 folder for Fusion360 CAD files and STL files for the 3D printed parts

## Installation
- Install the Arduino IDE
- Unzip this project in your ~/Arduino project directory, e.g., $HOME/Documents/Arduino/CoilWinder
- Install the remaining Arduino libraries using the Arduino IDE library manager
- Open the CoilWinder.ino file in the Arduino IDE, set the board type to Arduino UNO, compile and upload
