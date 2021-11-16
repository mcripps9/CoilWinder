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

//#define DIR 2
//#define STEP 3
#define DIR 6
#define STEP 7
#define SLEEP 4 // optional (just delete SLEEP from everywhere if not used)

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

typedef struct
{
  const char *description;
  unsigned  spindle_rpm;
  unsigned  sweep_time;
  unsigned  num_winds;
  unsigned  sweep_left;
  unsigned  sweep_right;
} COIL;

#define NUM_PROG  12
COIL coilArray[NUM_PROG] = {
  {"50 AWG\n3x2 75ohm", 250, 19200, 260, 1415, 1595 },
  {"50 AWG\n3x2 150ohm", 250, 19200, 520, 1415, 1595 },
  {"44.5 AWG\n3x3 50ohm", 480, 7911, 396, 1450, 1800},
  {"42.5 AWG\n3x3 40ohm", 480, 6270, 544, 1520, 1870},
  {"44 AWG\n3x3 150ohm", 100, 13326, 1338, 1415, 1595},
  {"48 AWG\n3x2 75 ohm", 80, 13326, 328, 1415, 1595},
  {"48 AWG\n3x3 150ohm", 80, 13326, 645, 1415, 1595},
  {"48 AWG\n2.4x2 75ohm", 80, 13326, 397, 1420, 1600 },
  {"42.5 AWG\nSpool", 480, 6270, 10000, 1450, 1800 },
  {"44 AWG\nSpool", 400, 18459, 10000, 1450, 1800 }, //7911
  {"49 AWG\nSpool", 60, 13326, 10000, 1450, 1800},
  {"Calibrate\nSweep", 480, 5000, 10000, 1400, 1600 }
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

unsigned program = 0;     // Which program is selected
unsigned menu_item = 0;   // Which menu item is selected (for paused menu)
unsigned wind_count = 0;
unsigned layer_count = 0;
boolean isPaused = false;

void setup() {
  unsigned button_press = 0;

  // Setup Stepper
    // if using enable/disable on ENABLE pin (active LOW) instead of SLEEP uncomment next line
  stepper.setEnableActiveState(LOW);
  stepper.disable();
  stepper.setSpeedProfile( stepper.CONSTANT_SPEED, 1, 1 ) ;
  stepper.begin(coilArray[program].spindle_rpm, MICROSTEPS);


  
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
  display.println(F("CoilWinder v1.0"));
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();

  // Setup rotary encoder
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RE_CLK, INPUT);
  pinMode(RE_DT, INPUT);
  lastStateCLK = digitalRead(RE_CLK);
  attachInterrupt(0, updateEncoder, CHANGE );
  attachInterrupt(1, updateEncoder, CHANGE );

  // Setup servo
  asyncServo.SetOutput(800, 1500, 2200);
  asyncServo.write(coilArray[program].sweep_left);
  asyncServo.Attach(SERVO_PIN);
  delay(2000);
  asyncServo.Detach();

  mode = SELECT_MODE;
  state = STARTING;
  //for ( int i = 0; i < 1000; ++i )
  //{
  //  if ( digitalRead( BUTTON_PIN ) == LOW )
   // {
   //    ++button_press;
   // }
   // delay(1);
  //}
  //if ( button_press > 500 )
  //{
  //  mode = CALIBRATE;
  //  state = STARTING;
  //}
}

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
        break;
    }
  }

  // Remember last CLK state
  lastStateCLK = currentStateCLK;
}

void guidingEye_at_left()
{
  //Serial.print("Left: " );
  //Serial.println(layer_count);
  layer_count++;
  asyncServo.Move(coilArray[program].sweep_right, coilArray[program].sweep_time, guidingEye_at_right );

}

void guidingEye_at_right()
{
  //Serial.println("Right");
  layer_count++;
  asyncServo.Move(coilArray[program].sweep_left, coilArray[program].sweep_time, guidingEye_at_left );
}

boolean buttonActive = false;
boolean longPressActive = false;
long buttonTimer = 0;
#define LONG_PRESS  2000
unsigned long abort_time_millis = 0;

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
/*  STATE old_state = state;
/*
  //Serial.println( state );
  if ( state == ABORTING )
  {
    //Serial.print("Diff: ");
    //Serial.println(millis() - abort_time_millis );
    if ( ( millis()  - abort_time_millis ) > 10000 ) {
      state = RUNNING;
    }
  }

  if ( digitalRead( BUTTON_PIN ) == LOW ) {
    if ( buttonActive == false ) {
      buttonActive = true;
      Serial.println( "Button Low");
      buttonTimer = millis();
    }
    if ( (millis() - buttonTimer > LONG_PRESS) && ( longPressActive == false )) {
      longPressActive = true;
      //if ( state == PROG_SEL ) {
      //  state = STARTING;
      //}
      //else
      if ( state == RUNNING) {
        state = ABORTING;
      }
      else if ( state == ABORTING ) {
        state = DONE;
      }
    }
  }
  else {
    if ( buttonActive == true ) {
      if ( longPressActive == true ) {
        longPressActive = false;
        Serial.println("Long off");
      }
      else {
        Serial.print("State: " );
        Serial.println( state );
        switch ( state ) {
          //case PROG_SEL:
          //  ++program;
          //  if ( program >= NUM_PROG ) {
          //    program = 0;
          //  }
          //  Serial.println( "Next prog" );
          //  break;
          case RUNNING:
            state = PAUSED;
            Serial.println("Pausing");
            break;
          case PAUSED:
            state = RUNNING;
            Serial.println("Unpausing");
            break;
        }
      }
      buttonActive = false;
    }
  }
*/
  switch ( state ) {
    //case PROG_SEL:
    //  display.clearDisplay();
    //  display.setCursor(0, 0);
    //  display.println(coilArray[program].description);
    //  display.display();
    //  break;
    /*
    case ABORTING:
      if ( !isPaused ) {
        asyncServo.Pause();
        abort_time_millis = millis();
        //    Serial.print("Abort time ");
        //    Serial.println(abort_time_millis);
      }
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Hold to\nabort -");
      display.println(wind_count);
      display.display();
      //Serial.println( millis() );

      break;
      */
    case PAUSED:
      //if ( old_state == RUNNING ) {
      //  asyncServo.Pause();
      //}
      isPaused = true;
      mode = PAUSED_MODE;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Paused");
      display.println(wind_count);
      display.display();
      break;
    case SHIFT_LEFT:
      coilArray[program].sweep_left -= 5;
      coilArray[program].sweep_right -= 5;
      state = RUNNING;
      break;
    case SHIFT_RIGHT:
      coilArray[program].sweep_left += 5;
      coilArray[program].sweep_right +=5 ;
      state = RUNNING;
      break;
    case STARTING:
      wind_count = coilArray[program].num_winds;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Starting");
      display.println(wind_count);
      display.display();
      Serial.println("START");
      asyncServo.Attach(SERVO_PIN);
      stepper.begin(coilArray[program].spindle_rpm, MICROSTEPS);
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
      asyncServo.write( 1500 );
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
      longPressActive = false;
      break;
  }
}

void calibrate_loop()
{
  unsigned wait_time_micros = 0;
  switch ( state )
  {
    case STARTING:
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Calibrating.....");
      display.display();
      Serial.println("START Calibrate");
      delay(1000);
      asyncServo.Attach(SERVO_PIN);
      stepper.begin(coilArray[program].spindle_rpm, MICROSTEPS);
      stepper.enable();
      guidingEye_at_left();
      stepper.startRotate(360);
      state = RUNNING;
      break;
    case RUNNING:
      wait_time_micros = stepper.nextAction();
      if (wait_time_micros <= 0) {
        stepper.startRotate( 360 );

      }
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(asyncServo.GetCurrentPosition());
      display.display();
      //if ( old_state == PAUSED || old_state == ABORTING ) {
      //  asyncServo.Resume();
      //}
      asyncServo.Update();
      break;
  }
}

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
    display.println(coilArray[program].description);
    display.display();
}

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
    case SELECT_MODE:
      select_loop();
      break;
    case PAUSED_MODE:
      paused_loop();
      break;
    case WIND_MODE:
      wind_loop();
      break;
    case CALIBRATE_MODE:
      calibrate_loop();
      break;
  }
}
