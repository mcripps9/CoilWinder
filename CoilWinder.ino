/*
   Simple stepper motor based coil winder

   Copyright (C)2021-2024, Michael Cripps

   This file may be redistributed under the terms of the Apache 2.0 license.
   A copy of this license has been included with this distribution in the file LICENSE.

   Requires these Arduino libraries:

*/
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <Servo.h>
#include "Types.h"

#define	VERSION	"v2.0.0"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
SSD1306AsciiWire display;
#define LINE_1_Y  0
#define LINE_2_Y  32

// Output pin for servo
#define SERVO_PIN 9
// this pin should connect to Ground for a button press
#define BUTTON_PIN 13
#define FOOTSW_PIN  12

// Default motor steps per revolution. Use 24 for 15 deg/step, 48 for 75 deg/step, or 200 for 18 deg/step
// The EEPROM will be initialized with this value, but can be changed later with the user interface
#define DEFAULT_MOTOR_STEPS 48
// Microstepping mode.  There is no value to microstepping for the winder, so we default it to off
#define MICROSTEPS 1

// Output pins for stepper motor control (STEP and DIR)
#define DIR 6
#define STEP 7

// Input pins for rotary encoder CLK and DT
#define RE_CLK  2
#define RE_DATA 3

#include "DRV8834.h"
#define STEPPER_DRIVER  DRV8834
//#include "A4988.h"
//#define STEPPER_DRIVER  A4988

Servo servo;

int8_t menu_item = 0;   // Which menu item is selected (for paused menu)
int8_t max_menu_item = 0;
unsigned program = 0;     // Which program is selected
unsigned wind_count = 0;
unsigned layer_count = 0;
unsigned layer_wind_count = 0;
boolean servo_direction = true;
boolean displayChanged = true;
boolean dataChanged = false;

HW_CONFIG config = { 2, 24, 1 };
#define CONFIG_ADDR   0   // Offset of configuration data in EEPROM
#define COILDATA_ADDR 64  // Offset of coil program data in EEPROM

STEPPER_DRIVER *stepper = NULL;
/*
  uint16_t  inner_diameter; (mm x 100 for display only)
  uint16_t  width_mm; (mm x 100 for display only)
  uint16_t  wire_type; (AWG x 10 for display only)
  uint16_t  ohms; (for display only)
  uint16_t  spindle_rpm;
  uint16_t  num_winds; (total winds)
  uint16_t  winds_per_layer;
  uint16_t  servo_left_pos; (servo position in us at leftmost extent)
  uint16_t  width_us; (added to servo_left_pos to determine rightmost extent)
*/
COIL_DATA coilData[] = {
  { 300, 200, 480, 75, 80, 328, 30, 1000, 1000 },
  { 300, 300, 480, 150, 80, 645, 90, 1200, 600 },
  { 240, 200, 480, 75, 80, 397, 30, 1000, 1000 },
  { 300, 300, 425, 40, 50, 554, 90, 1200, 600 },
  { 300, 300, 440, 50, 63, 396, 90, 1200, 600 },
  { 300, 300, 440, 150, 53, 1338, 90, 1200, 600 },
  { 240, 200, 480, 75, 80, 397, 30, 1000, 1000 },
  { 300, 300, 425, 40, 50, 554, 90, 1200, 600 },
  { 300, 300, 440, 50, 63, 396, 90, 1200, 600 },
  { 300, 600, 440, 9999, 150, 10000, 90, 1200, 600 }, 
  { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

STATE state = STARTING;
MODE mode = MAIN_MODE;

#include "Menus.h"

static uint8_t prevNextCode = 0;
static uint16_t store=0;
#define FAST_ROTATE 60

// Read rotary encoder knob direction with debouncing
// A vald CW or  CCW move returns 1, invalid returns 0.
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};
  static unsigned long last_move = 0;
  static int8_t last_dir = 0;
  int8_t ret = 0;

  prevNextCode <<= 2;
  if (digitalRead(RE_DATA)) prevNextCode |= 0x02;
  if (digitalRead(RE_CLK)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      if ((store&0xff)==0x2b) {
        ret = -1;
      }
      else if ((store&0xff)==0x17) {
        ret = 1;
      }
   }
   if ( ret != 0 ) {
    int8_t mult = 1;
    unsigned long now = millis();
    if ( last_dir == ret ) {
      if ( ( now - last_move ) < FAST_ROTATE ) {
        mult = 10;
        //Serial.println( now - last_move );
      }

    }
    last_move = now;
    last_dir = ret;
    ret = ret * mult;
   }
   return ret;
}

/*
  Update the display in response to user input
*/
void display_menu()
{
  //Serial.println( mode );
  if ( displayChanged && ( menus[mode].items != NULL ) ) {
    //Serial.println(menus[mode].items[menu_item].description);

    display.clear();
    if ( ( mode == SELECT_MODE ) && ( coilData[menu_item].inner_diameter != 0 ) ) {
      COIL_DATA *d = &coilData[menu_item];

      String str1 = String(d->wire_type / 10.0, 1 );
      display.print( str1.c_str() ); display.print( " AWG " );
      display.print( d->ohms ); display.print(" ohm"); //display.clearToEOL();
      display.setCursor( 0, 2 );
      String str2 = String(d->inner_diameter / 100.0, 2 );
      display.print( str2.c_str() ); display.print( " mm i.d. x " );
      String str3 = String(d->width_mm / 100.0, 2 );
      display.print( str3.c_str() ); display.print( " mm"); //display.clearToEOL();
    }
    else {
      if ( ( mode == MAIN_MODE ) && dataChanged ) {
        display.print("* ");
      }
      display.println(menus[mode].items[menu_item].description);
    }
    int val_offset = menus[mode].items[menu_item].value_offset;
    void *val_ptr = menus[mode].value_ptr;
    if ( val_ptr && ( val_offset != -1 ) ) {
      int value = *static_cast<int *>(menus[mode].value_ptr + val_offset);
      if ( state == RUNNING || state == SHIFT_LEFT || state == SHIFT_RIGHT ) {
        display.print( "+ " );
      }
      display.println( value );
    }
    displayChanged = false;
  }
  return;
}

/*
  Write two fixed lines to the display
*/
void display_strings( char *line1, char* line2 )
{
  display.clear();
  display.println( line1 );
  display.println( line2 );
}

void set_mode( MODE new_mode, STATE new_state )
{
  displayChanged = true;
  state = new_state;
  if ( mode != new_mode ) {
    // Remember last selected program
    if ( new_mode == SELECT_MODE ) {
      menu_item = program;
    }
    else {
      menu_item = 0;
    }
  }
  mode = new_mode;
  //Serial.print("set_mode: "); Serial.print( mode ); Serial.print( " " ); Serial.println( state );
  max_menu_item = menus[mode].num_items;
  display_menu();
}
// Set specific mode, set state based on current menu selection
void set_mode( MODE mode )
{
  set_mode( mode,
    menus[mode].items[menu_item].new_state );
}
// Set mode and state based on current menu selection
void set_mode()
{
  set_mode( menus[mode].items[menu_item].new_mode,
    menus[mode].items[menu_item].new_state );
}

/*
  Load configuration data and coil data from EEPROM.
  If the EEPROM has not been initialized yet, use the hardcoded defaults
*/
void load_eeprom() {
  // Uncomment to reset to factory defaults
  //EEPROM.write( CONFIG_ADDR, 255 );
  EEPROM.get( CONFIG_ADDR, config );
  //Serial.println( config.version );
  // -1 means EEPROM hasn't been initialized, so set defaults
  if ( config.version == 255 )
  {
    config.version = 2;
    config.steps_per_rev = DEFAULT_MOTOR_STEPS;
    dataChanged = true;
  }
  else {
    EEPROM.get( COILDATA_ADDR, coilData );
  }
}

/*
  Save hardware configuration and coil program data to EEPROM.
  Uses 'put' functionality of EEPROM library to only write changed data
*/
void save_eeprom() {
  display_strings( "Saving data", "to EEPROM...");
  EEPROM.put( CONFIG_ADDR, config );
  EEPROM.put( COILDATA_ADDR, coilData );
  delay( 1000 );
  void(* resetFunc) (void) = 0;
  resetFunc();
}

void setup() {
  load_eeprom();
  // Setup Stepper motor driver
  stepper = new STEPPER_DRIVER(config.steps_per_rev, DIR, STEP );
  stepper->setEnableActiveState(LOW);
  stepper->disable();
  stepper->setSpeedProfile( stepper->CONSTANT_SPEED, 1, 1 ) ;
  stepper->begin(80, MICROSTEPS);
  
  // Setup Serial port for debugging
  Serial.begin(115200);
  Serial.println("RESET");

  // Setup display
  Wire.begin();
  Wire.setClock(400000L);
  display.begin(&Adafruit128x32, SCREEN_ADDRESS);
  display.setFont(Arial14);
  display.set1X();
  display.clear();
  display_strings( "CoilWinder", VERSION );

  // Setup rotary encoder
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FOOTSW_PIN, INPUT_PULLUP);
  pinMode(RE_CLK, INPUT);
  pinMode(RE_CLK, INPUT_PULLUP);
  pinMode(RE_DATA, INPUT);
  pinMode(RE_DATA, INPUT_PULLUP);

  // Setup servo to home position
  servo.writeMicroseconds( coilData[program].servo_left_pos );
  servo.attach(SERVO_PIN);
  delay(2000);

  // Initialize the state machine
  set_mode( MAIN_MODE, STARTING );
}

/*
  Wind mode processing loop.  This loop is a state machine that handles different states during a coil wind, including
  when the winding has been paused.

  It handles display updates, checking for button presses, turning the stepper motor on or off, 
  and turning the guiding eye servo on or off.

  Transitions to PAUSE_MODE when the button is pressed, or SELECT_MODE when the coil wind is done
*/
void wind_loop( boolean buttonClicked ) {
  unsigned wait_time_micros;
  static int last_servo_pos;


  if ( buttonClicked )  {
    //Serial.println( "Wind button clicked");
    state = PAUSED;
  }

  switch ( state ) {
    case DONE:
      Serial.println("DONE");
      stepper->disable();
      servo.writeMicroseconds( coilData[program].servo_left_pos );
      display_strings("Done!","");
      delay(3000);
      set_mode( SELECT_MODE, STARTING );
      break;
    case PAUSED:
      //Serial.println("PAUSED");
      set_mode( PAUSED_MODE, STARTING );
      break;
    case SHIFT_LEFT:
      coilData[program].servo_left_pos -= 5;
      state = RUNNING;
      break;
    case SHIFT_RIGHT:
      coilData[program].servo_left_pos += 5;
      state = RUNNING;
      break;
    case STARTING:
      //Serial.println("STARTING");
      //wind_count = coilPrograms[program].num_winds;
      wind_count = coilData[program].num_winds;
      layer_wind_count = coilData[program].winds_per_layer;
      layer_count = 1;
      servo_direction = true;
      display.clear();
      display.println("Starting wind");
      display.print("Remaining: "); display.println(wind_count);
      //Serial.println("START");
      last_servo_pos = coilData[program].servo_left_pos;
      servo.writeMicroseconds( last_servo_pos );
      stepper->begin(coilData[program].spindle_rpm, MICROSTEPS);
      stepper->enable();
      stepper->startRotate(360);
      state = RUNNING;
      break;
    case RUNNING:
      //Serial.println("RUNNING");
      wait_time_micros = stepper->nextAction();
      if (wait_time_micros <= 0) {
        // We have completed a wind
        wind_count--;
        layer_wind_count--;
        display.setCursor(0,0);
        display.print("Layer: "); display.print( layer_count ); display.print(" Wind: "); display.print( layer_wind_count ); display.println("    ");
        display.print("Remaining: "); display.print(wind_count); display.println("     ");
        if ( wind_count <= 0  ) {
          state = DONE;
        }
        else if ( layer_wind_count <= 0 ) {
          ++layer_count;
          layer_wind_count = coilData[program].winds_per_layer;
          if ( servo_direction ) {
            servo_direction = false;
          }
          else {
            servo_direction = true;
          }
        }
        stepper->startRotate( 360 );
      }
      // Calculate servo position based on where we are in the layer
      float remaining = 0.0;
      if ( config.smooth_servo ) {
        // If configured for smooth servo moves, we updating after each step of the spindle motor.  Otherwise, we only
        // update after a complete spindle revolution
        remaining = 1 - ( stepper->getStepsRemaining() / static_cast<float>(config.steps_per_rev) );
      }
      float pos1 = ( coilData[program].winds_per_layer - layer_wind_count + remaining ) / static_cast<float>(coilData[program].winds_per_layer);
      int servo_pos = last_servo_pos;
      //Serial.print( coilData[program].winds_per_layer ); Serial.print(" - "); Serial.println( layer_wind_count );
      //Serial.print( remaining ); Serial.print(" ");
      if ( servo_direction ) {
        float pos = static_cast<float>( pos1 * coilData[program].width_us )
          + coilData[program].servo_left_pos;
        servo_pos = static_cast<int>(pos);
      }
      else {
        float pos = ( coilData[program].servo_left_pos + coilData[program].width_us ) - 
          ( pos1 * static_cast<float>(coilData[program].width_us) );
        servo_pos = static_cast<int>(pos);
      }
      // Only update the servo if the position has changed since last iteration
      if ( servo_pos != last_servo_pos ) {
        //Serial.println( servo_pos );
        servo.writeMicroseconds( servo_pos );
        last_servo_pos = servo_pos;
      }      
      break;

  }
}

/*
  Process events when in hardware setup mode - SETUP_MODE
*/
void setup_loop( boolean buttonClicked, int8_t knob )
{
    if ( knob ) {
      if ( state == RUNNING ) {
        dataChanged = true;     // Set global flag to warn user that there is unsaved data
      }
    }
    if ( buttonClicked ) {
      switch ( state ) {
        // STARTING state means parameters are being selected
        case STARTING:
          set_mode();
          break;
        // RUNNING state means value is being changed
        case RUNNING:
          set_mode( SETUP_MODE, STARTING );
          break;
      }
    };

    switch ( state )
    {
      // PAUSED state means reset the device
      case ABORTING:
        EEPROM.write( CONFIG_ADDR, 255 );
        void(* resetFunc) (void) = 0;
        resetFunc();
        break;
    }
    display_menu();
}

/*
  Process events when in coil select mode - SELECT_MODE.
  Used when selecting a coil for winding (state == STARTING) or for editing (state == EDITING)
*/
void select_loop( boolean buttonClicked, int8_t knob )
{
  if ( knob ) {
    // When selecting for winding, position the servo at the leftmost extent
    //if ( state == STARTING ) {
      // Little hack to not move the servo when 'Return to main' is selected
      if ( coilData[menu_item].servo_left_pos != 0 ) {
        servo.writeMicroseconds( coilData[menu_item].servo_left_pos );
      }
    //}
  }
  if ( buttonClicked ) {
    // If "Return to main" selected, do that
    //Serial.println("select_loop button clicked");
    //Serial.print( "Mode is "); Serial.print(mode); Serial.print(" state "); Serial.print( state ); Serial.print( " menu_item "); Serial.println( menu_item );
    if ( coilMenu[menu_item].new_mode == MAIN_MODE ) {
      set_mode();
      return;
    }
    // Otherwise, wind or edit
    program = menu_item;
    //Serial.print( "State is "); Serial.println( state );
    switch ( state )
    {
      // State when selecting for a wind
      case STARTING:
        set_mode( WIND_MODE, STARTING );
        break;
      // State when selecting for an edit
      case EDITING:
        menus[EDIT_MODE].value_ptr = &coilData[program];
        set_mode( EDIT_MODE, STARTING );
        break;
      default:
        Serial.print( "select_loop invalid state "); Serial.print( state );
    }
  }
  display_menu();
}

/*
  Process events when editing the parameters of a coil program mode - EDIT_MODE
*/
void edit_loop( boolean buttonClicked, int8_t knob )
{
  if ( knob ) {
    dataChanged = true;   // Value has changed, so set global flag to warn user that there are unsaved changes
    switch ( state )
    {
      // Special states used to adjust the servo left position and servo right position
      case SHIFT_LEFT:
        if ( coilData[program].servo_left_pos != 0 ) {
          servo.writeMicroseconds( coilData[program].servo_left_pos );
        }
        break;
      case SHIFT_RIGHT:
        if ( coilData[program].servo_left_pos != 0 ) {
          servo.writeMicroseconds( coilData[program].servo_left_pos + coilData[program].width_us );
        }
        break;
    }
  }

  if ( buttonClicked ) {
    // If "Return to main" selected, do that
    //Serial.println("edit_loop button clicked");
    //Serial.print( "Mode is "); Serial.print(mode); Serial.print(" state "); Serial.print( state ); Serial.print( " menu_item "); Serial.println( menu_item );
    switch ( state )
    {
      case SHIFT_LEFT:
      case SHIFT_RIGHT:
      case RUNNING:
        set_mode( EDIT_MODE, STARTING );
        break;
      case STARTING:
        //Serial.println("edit_loop STARTING");
        set_mode();
        if ( ( state == SHIFT_LEFT ) && ( coilData[program].servo_left_pos != 0 ) ) {
          servo.writeMicroseconds( coilData[program].servo_left_pos );
        }
        else if ( ( state == SHIFT_RIGHT ) && ( coilData[program].servo_left_pos != 0 )  ){
          servo.writeMicroseconds( coilData[program].servo_left_pos + coilData[program].width_us );
        }
        break;
    };
  }
  display_menu();
}

#define DEBOUNCE_DELAY  50
bool buttonPressed()
{
  static bool buttonState = false;
  static int lastButtonState = LOW;
  static unsigned long lastDebounceTime = millis();
  bool ret = false;

  int reading = digitalRead( BUTTON_PIN );
  if ( reading != LOW ) {
    reading = digitalRead(FOOTSW_PIN);
  }
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  lastButtonState = reading;
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      if ( buttonState ) {
        buttonState = false;
        ret = true;
      }
      else {
        buttonState = true;
      }
    }
  }
  return ret;
}

void loop()
{
    boolean buttonClicked = false;
    static boolean knobTurned = false;
    int8_t knob=0;

    // Read rotary encoder state
    if( knob=read_rotary() ) {
      //c +=val;
      switch ( state )
      {
        // These states are used when scrolling through menu items
        case STARTING:
        case EDITING:
          //Serial.print( mode ); Serial.print( " " ); Serial.print( menu_item ); Serial.print( " " ); Serial.println( max_menu_item );
          menu_item = menu_item + knob;
          if ( menu_item >= max_menu_item ) {
            menu_item = 0;
          }
          if ( menu_item < 0 ) {
            menu_item = max_menu_item - 1;
          }
          displayChanged = true;
          break;
        // These states are used when editing a parameter value.  value_ptr points to a structure or variable that contains the parameter
        // being edited, and val_offset is the offset location in the structure for the parameter (or 0 for a variable)
        // The SHIFT_LEFT and SHIFT_RIGHT states are special, used for adjusting the servo endpoints
        case RUNNING:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
          int val_offset = menus[mode].items[menu_item].value_offset;
          if ( val_offset != -1 ) {
            int *valptr = static_cast<int *>(menus[mode].value_ptr + val_offset);
            *valptr = *valptr + knob;
            displayChanged = true;
          }
          break;
      }
    }
    //Serial.print(knob); Serial.println("");
    // Read button state
    buttonClicked = buttonPressed();

  //Serial.println( mode );
  switch ( mode )
  {
    // Main menu mode
    case MAIN_MODE:
      if ( buttonClicked ) {
        if ( menus[mode].items[menu_item].new_state == DONE ) {
          // WRITE TO EEPROM
          save_eeprom();
          set_mode( MAIN_MODE, STARTING );
          break;
        }
        else if ( menus[mode].items[menu_item].new_state == ABORTING ) {
          void(* resetFunc) (void) = 0;
          resetFunc();
        }
        else {
          set_mode();
        }
      }
      display_menu();
      break;
    // Mode when selecting the coil program for either winding or editing
    case SELECT_MODE:
      select_loop( buttonClicked, knob );
      break;
    // Mode when editing a coil program
    case EDIT_MODE:
      edit_loop( buttonClicked, knob );
      break;
    // Mode when paused during a program (invoked by clicking the button)
    case PAUSED_MODE:
      if ( buttonClicked ) {
        //Serial.println("PAUSED_MODE clicked");
        set_mode();
      }
      display_menu();
      break;
    // Mode when a coil is being wound
    case WIND_MODE:
      wind_loop( buttonClicked );
      break;

    // Mode when in hardware setup mode
    case SETUP_MODE:
      setup_loop( buttonClicked, knob );
      break;
  }
}
