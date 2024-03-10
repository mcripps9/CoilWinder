# Change Log
Notable changes to this project are documented here

## v2.0.0
Complete rewrite of the code to support a more sophisticated menu system,
in-device editing of coil parameters and hardware settings, and use of
EEPROM to permanently save changes.

### Added
- New menu system including Edit Coils menu to edit coil programs,
  Hardware Setup menu to configure stepper motor and servo behavior
- Menu based editing of individual coil programs within the device without
  needing to re-upload the software from the Arduino IDE
- Saving of configuration and coil program data to EEPROM for
  persistent storage.  And asterisk (*) on the main menu alerts you that
  there is changed data that needs saving
- Reset all data option
- Support for a second pushbutton or footswitch connected to digital pin 12
  as an alternative to the rotary encoder button.  Either can be used.
- Servo moves when selecting coil programs or when editing servo endpoints
  to help align to the coil former

### Changed
* Changed parameters of coil programs.  They are now:
** Coil Inner Diameter in mm x 10 (for display only)
** Coil Width in mm x 10 (for display only)
** Wire AWG x 10 (for display only)
** Coil Ohms (for display only)
** Spindle RPM
** Total number of winds
** Winds per layer
** Servo left position in us
** Servo width in us (this is added to Servo left position to determine rightmost servo position)
* Switched to SSD1306Ascii library to save memory
* Better debouncing of rotary encoder movements and detection of fast moves to increment/decrement editable values by 10

## v1.1.0

### Added
- Added function to abort "Calibrate Sweep" mode by clicking the button

### Changed
- Changed the mapping of sweep_left and sweep_right values to actual
  servo pulse widths. The values in the coilProgram array range from
  0 to 1400.  These will be normalized to an actual pulse width range using the
  MIN_SERVO and MAX_SERVO constants.  This lets you tweak the servo pulse
  timings globally without having to change the coil programs.
- When scrolling through programs, after a wind is complete, or after
  a wind is aborted the servo will automatically move to the left extent.
- Changed the "Calibrate Sweep" program to a simpler live adjustment of
  the servo position.  Rotate the knob and the servo rotates to match.
  Note down the values of the left and right extents, and you have the
  values for sweep_left and sweep_right
- Removed commented out blocks and other unused code

### Fixed
- Fixed issue where "Calibrate Sweep" program was not program 0 so the
  first program in the list didn't work, but instead ran the Calibrate Sweep
  program.  On power up, program #1 is the default.

## v1.0.0

Initial release
