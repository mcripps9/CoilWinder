/*
   Simple stepper motor based coil winder

   Copyright (C)2021, Michael Cripps

   This file may be redistributed under the terms of the Apache 2.0 license.
   A copy of this license has been included with this distribution in the file LICENSE.

   Requires these Arduino libraries:

*/
#include <Arduino.h>
#include <AsyncServoLib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define	VERSION	"v1.1"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Output pin for servo
#define SERVO_PIN 9
// this pin should connect to Ground for a button press
#define BUTTON_PIN 13

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 24
// Microstepping mode. If you hardwired it to save pins, set to the same value here.
#define MICROSTEPS 1
// was 16

// Output pins for stepper motor control (STEP and DIR)
#define DIR 6
#define STEP 7

// Input pins for rotary encoder CLK and DT
#define RE_CLK  2
#define RE_DT  3

int counter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir = "";

// DRV8834 board mimics the A4988
#include "A4988.h"
#define MS1 10
#define MS2 11
#define MS3 12
A4988 stepper(MOTOR_STEPS, DIR, STEP ); //, SLEEP, MS1, MS2, MS3);

AsyncServo asyncServo;
/* 
  Constants defining the input range for the guiding eye servo position.
  These are the ranges of the sweep_left and sweep_right values in the coilProgram array.
  They will be normalized to actual servo pulse width values in us by the AsyncServoLib library
*/
#define MIN_SWEEP 0
#define MID_SWEEP 700
#define MAX_SWEEP 1400
/*
  Constants defining the actual pulse width in us that your servo will tolerate 
  as the minimum (left) position, middle, and maximum (right) position.
*/
#define MIN_SERVO 800
#define MID_SERVO 1500
#define MAX_SERVO 2200

typedef struct
{
  const char *description;  // Text shown on display
  unsigned  spindle_rpm;    // RPM of spindle
  unsigned  sweep_time;     // Time in ms to sweep the guiding eye servo from the left to the right extents
  unsigned  num_winds;      // Total number of winds in the coil
  unsigned  sweep_left;     // Servo pulse width in us of the left extent of the guiding eye servo
  unsigned  sweep_right;    // Servo pulse width in us of the right extent of the guiding eye servo
} COIL;

#define NUM_PROG  12
COIL coilPrograms[NUM_PROG] = {
  {"Calibrate\nSweep", 480, 5000, 10000, MID_SWEEP, MID_SWEEP }, // 1400, 1600
  {"50 AWG\n3x2 75ohm", 250, 19200, 260, 610, 790 },
  {"50 AWG\n3x2 150ohm", 250, 19200, 520, 610, 790 },
  {"44.5 AWG\n3x3 50ohm", 480, 7911, 396, 525, 875},
  {"42.5 AWG\n3x3 40ohm", 480, 6270, 544, 525, 875},
  {"44 AWG\n3x3 150ohm", 100, 13326, 1338, 610, 790},
  {"48 AWG\n3x2 75 ohm", 80, 13326, 328, 610, 790},
  {"48 AWG\n3x3 150ohm", 80, 13326, 645, 610, 790},
  {"48 AWG\n2.4x2 75ohm", 80, 13326, 397, 610, 790 },
  {"42.5 AWG\nSpool", 480, 6270, 10000, 525, 875 },
  {"44 AWG\nSpool", 400, 18459, 10000, 525, 875 }, //7911
  {"49 AWG\nSpool", 60, 13326, 10000, 525, 875}
};

typedef enum { SELECT_MODE, WIND_MODE, CALIBRATE_MODE, PAUSED_MODE } MODE;
typedef enum  {  STARTING, RUNNING, SHIFT_LEFT, SHIFT_RIGHT, PAUSED, ABORTING, DONE } STATE;

typedef struct
{
  const char *description;
  STATE  next_state;
} MENU;

#define NUM_PAUSED_MENU 5
const MENU pauseMenu[NUM_PAUSED_MENU] = {
  {"Paused...", PAUSED },
  {"Press\nto resume", RUNNING },
  {"Shift\nLeft", SHIFT_LEFT },
  {"Shift\nRight", SHIFT_RIGHT },
  {"Press\nto abort", DONE }
};


STATE state = STARTING;
MODE mode = SELECT_MODE;



unsigned calibrate_position = MID_SWEEP;

unsigned program = 1;     // Which program is selected
unsigned menu_item = 0;   // Which menu item is selected (for paused menu)
unsigned wind_count = 0;
unsigned layer_count = 0;
boolean isPaused = false;

void setup() {
  unsigned button_press = 0;

  // Setup Stepper motor driver
  stepper.setEnableActiveState(LOW);
  stepper.disable();
  stepper.setSpeedProfile( stepper.CONSTANT_SPEED, 1, 1 ) ;
  stepper.begin(coilPrograms[program].spindle_rpm, MICROSTEPS);
  
  Serial.begin(115200);
  Serial.println("RESET");

  // Setup OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CoilWinder"); display.println(VERSION);
  display.display();

  // Setup rotary encoder
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RE_CLK, INPUT);
  pinMode(RE_DT, INPUT);
  lastStateCLK = digitalRead(RE_CLK);
  attachInterrupt(0, updateEncoder, CHANGE );
  attachInterrupt(1, updateEncoder, CHANGE );

  // Setup servo
  // The asyncServoLib normalizes the input position to an actual set of pulse widths.
  asyncServo.SetInput( MIN_SWEEP, MID_SWEEP, MAX_SWEEP);     // Max Left, center, and max right extents in coilPrograms
  asyncServo.SetOutput(MIN_SERVO, MID_SERVO, MAX_SERVO);     // Actual left, center, and right pulse widths in us that are sent to servo
  asyncServo.write(coilPrograms[program].sweep_left);
  asyncServo.Attach(SERVO_PIN);
  delay(2000);
  //asyncServo.Detach();

  // Initialize the state machine
  mode = SELECT_MODE;
  state = STARTING;
}

/*
  Interrupt routine that handles rotation of the rotary encoder
*/
void updateEncoder() {
  // Read the current state of RE_CLK
  currentStateCLK = digitalRead(RE_CLK);

  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){

    switch (mode )
    {
      case PAUSED_MODE:
        // If the DT state is the same as the CLK state then
        // the encoder is rotating CCW so decrement
        if (digitalRead(RE_DT) == currentStateCLK) {
          if ( menu_item == 0 ) {
            menu_item = NUM_PAUSED_MENU;
          }
          menu_item --;
        } else {
        // Encoder is rotating CW so increment
          menu_item ++;
          if ( menu_item >= NUM_PAUSED_MENU ) {
            menu_item = 0;
          }
        }
        break;
      case SELECT_MODE:
        // If the DT state is the same as the CLK state then
        // the encoder is rotating CCW so decrement
        if (digitalRead(RE_DT) == currentStateCLK) {
          if ( program == 0 ) {
            program = NUM_PROG;
          }
          program --;
        } else {
        // Encoder is rotating CW so increment
          program ++;
          if ( program >= NUM_PROG ) {
            program = 0;
          }
        }
        case CALIBRATE_MODE:
        // If the DT state is the same as the CLK state then
        // the encoder is rotating CCW so decrement
        if (digitalRead(RE_DT) == currentStateCLK) {
          if ( calibrate_position == 0 ) {
            calibrate_position = MAX_SWEEP;
          }
          else {
            calibrate_position --;
          }
        } else {
        // Encoder is rotating CW so increment
          calibrate_position ++;
          if ( calibrate_position >= MAX_SWEEP ) {
            calibrate_position = 0;
          }
        }
        break;
    }
  }

  // Remember last CLK state
  lastStateCLK = currentStateCLK;
}

/*
  This routine is called by the AsyncServoLib when the guiding eye hits the left extent.  It tells the asyncServo object
  to start moving towards the right extent
*/
void guidingEye_at_left()
{
  //Serial.print("Left: " );
  //Serial.println(layer_count);
  layer_count++;
  asyncServo.Move(coilPrograms[program].sweep_right, coilPrograms[program].sweep_time, guidingEye_at_right );

}

/*
  This routine is called by the AsyncServoLib when the guding eye hits the right extent.  It tells the asyncServo object
  to start moving towards the left extent
*/
void guidingEye_at_right()
{
  //Serial.println("Right");
  layer_count++;
  asyncServo.Move(coilPrograms[program].sweep_left, coilPrograms[program].sweep_time, guidingEye_at_left );
}

boolean buttonActive = false;


/*
  Wind mode processing loop.  This loop is a state machine that handles different states during a coil wind, including
  when the winding has been paused.

  It handles display updates, checking for button presses, turning the stepper motor on or off, 
  and turning the guiding eye servo on or off.

  Transitions to PAUSE_MODE when the button is pressed, or SELECT_MODE when the coil wind is done
*/
void wind_loop() {
  unsigned wait_time_micros;

  if ( digitalRead( BUTTON_PIN ) == LOW ) {
     buttonActive = true;
  }
  else {
    if ( buttonActive ) {
      asyncServo.Pause();
      mode = PAUSED_MODE;
      isPaused = true;
      buttonActive = false;
      return;
      //GOTO PAUSE MENU
    }
  }

  switch ( state ) {
    case PAUSED:
      isPaused = true;
      mode = PAUSED_MODE;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Paused");
      display.println(wind_count);
      display.display();
      break;
    case SHIFT_LEFT:
      coilPrograms[program].sweep_left -= 5;
      coilPrograms[program].sweep_right -= 5;
      state = RUNNING;
      break;
    case SHIFT_RIGHT:
      coilPrograms[program].sweep_left += 5;
      coilPrograms[program].sweep_right +=5 ;
      state = RUNNING;
      break;
    case STARTING:
      wind_count = coilPrograms[program].num_winds;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Starting");
      display.println(wind_count);
      display.display();
      Serial.println("START");
      asyncServo.write(coilPrograms[program].sweep_left);
      asyncServo.Attach(SERVO_PIN);
      stepper.begin(coilPrograms[program].spindle_rpm, MICROSTEPS);
      stepper.enable();
      guidingEye_at_left();
      stepper.startRotate(360);
      state = RUNNING;
      break;
    case RUNNING:
      if ( isPaused == true ) {
        asyncServo.Resume();
        isPaused = false;
      }
      wait_time_micros = stepper.nextAction();
      if (wait_time_micros <= 0) {
        stepper.startRotate( 360 );
        wind_count--;
        //Serial.print("Layer: ");
        //Serial.print(layer_count);
        //Serial.print(" Wind: ");
        //Serial.println(wind_count);
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(wind_count);
        display.display();
      }
      if ( isPaused == true ) {
        asyncServo.Resume();
        isPaused = false;
      }
      asyncServo.Update();

      if ( wind_count <= 0  ) {
        state = DONE;
      }
      break;
    case DONE:
      stepper.disable();
      asyncServo.Stop();
      asyncServo.write( coilPrograms[program].sweep_left );
      Serial.println("Done");
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Done!");
      display.display();
      delay(5000);
      state = STARTING;
      mode = SELECT_MODE;
      asyncServo.Detach();
      buttonActive = false;
      break;
  }
}

/*
  Calibrate mode processing loop.  This mode is used to determine the exact values for the left and right extents of the
  guiding eye to be used as the 'sweep_left' and 'sweep_right' values in the coilPrograms coil program.

  The value on the display is the current servo position in microseconds.

  Clicking the button will stop calibration mode and go back to the main menu.
*/
void calibrate_loop()
{
  unsigned wait_time_micros = 0;

  if ( digitalRead( BUTTON_PIN ) == LOW ) {
     buttonActive = true;
  }
  else {
    if ( buttonActive ) {
      asyncServo.Pause();
      mode = WIND_MODE;
      state = DONE;
      buttonActive = false;
      return;
      //GOTO WIND_MODE and stop the program
    }
  }
  switch ( state )
  {
    case STARTING:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Calibrating.....");
      display.display();
      Serial.println("START Calibrate");
      delay(1000);
      asyncServo.write(0);
      asyncServo.Attach(SERVO_PIN);
      //stepper.begin(coilPrograms[program].spindle_rpm, MICROSTEPS);
      //stepper.enable();
      //guidingEye_at_left();
      //stepper.startRotate(360);
      state = RUNNING;
      break;
    case RUNNING:
      //wait_time_micros = stepper.nextAction();
      //if (wait_time_micros <= 0) {
      //  stepper.startRotate( 360 );
      //}
      asyncServo.write( calibrate_position );
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(asyncServo.GetCurrentPosition());
      display.display();
      //if ( old_state == PAUSED || old_state == ABORTING ) {
      //  asyncServo.Resume();
      //}
      //asyncServo.Update();
      break;
  }
}

/*
  Main menu processing loop.  Scroll through the programs with the knob, and listen for button press events
  to select the program and start a wind, or to enter calibration mode to get the values for the guiding eye extents

  Transitions to either WIND_MODE for a coil wind for CALIBRATE_MODE to calibrate the guiding eye extents
*/
void select_loop()
{
    if ( digitalRead( BUTTON_PIN ) == LOW ) {
      if ( buttonActive == false ) {
        buttonActive = true;
        Serial.println( "Button pressed");
        //buttonTimer = millis();
      }
    }
    else {
      if ( buttonActive == true ) {
        buttonActive = false;
        Serial.println( "Button released" );
        if ( program == 0 ) {
          mode = CALIBRATE_MODE;
        }
        else {
          mode = WIND_MODE;
        }
        state = STARTING;
      }
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(coilPrograms[program].description);
    asyncServo.write(coilPrograms[program].sweep_left);
    display.display();
}

/*
  Puased menu processing.  Scroll through the paused menu options, and check for button presses.
  The paused menu is invoked when you click the button during a winding program, and lets you either abort the
  wind, or slide the guiding eye extents left or right if your widing form is not correctly aligned to the guiding eye.

  Transitions back to WIND_MODE after a button click.
*/
void paused_loop()
{
    if ( digitalRead( BUTTON_PIN ) == LOW ) {
      if ( buttonActive == false ) {
        buttonActive = true;
        Serial.println( "Button pressed");
        //buttonTimer = millis();
      }
    }
    else {
      if ( buttonActive == true ) {
        buttonActive = false;
        Serial.println( "Button released" );
        state = pauseMenu[menu_item].next_state;
        mode = WIND_MODE;
        menu_item = 0;
      }
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(pauseMenu[menu_item].description);
    display.display();
}

void loop()
{
  switch ( mode )
  {
    // Mode when selecting the program
    case SELECT_MODE:
      select_loop();
      break;
    // Mode when paused during a program (invoked by clicking the button)
    case PAUSED_MODE:
      paused_loop();
      break;
    // Mode when a coil is being wound
    case WIND_MODE:
      wind_loop();
      break;
    // Mode used by program 0 to calibrate the guiding eye sweep extents
    case CALIBRATE_MODE:
      calibrate_loop();
      break;
  }
}
