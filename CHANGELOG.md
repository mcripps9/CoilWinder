# Change Log
Notable changes to this project are documented here

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
