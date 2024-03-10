#ifndef _Menus_h
#define _Menus_h

const MenuItem coilMenu[] = {
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {NULL, WIND_MODE, STARTING, -1 },
  {"Return to Main", MAIN_MODE, STARTING, -1 }
};

const MenuItem coilEditMenu[] = {
  {"Coil I.D. (mm x100)", EDIT_MODE, RUNNING, offsetof( COIL_DATA, inner_diameter ) },
  {"Coil Width (mm x100)", EDIT_MODE, RUNNING, offsetof( COIL_DATA, width_mm ) },
  {"Wire AWG x10", EDIT_MODE, RUNNING, offsetof( COIL_DATA, wire_type ) },
  {"Ohms", EDIT_MODE, RUNNING, offsetof( COIL_DATA, ohms ) },
  {"Spindle RPM", EDIT_MODE, RUNNING, offsetof( COIL_DATA, spindle_rpm ) },
  {"Total Winds", EDIT_MODE, RUNNING, offsetof( COIL_DATA, num_winds ) },
  {"Winds/Layer", EDIT_MODE, RUNNING, offsetof( COIL_DATA, winds_per_layer ) },
  {"Servo Left (us)", EDIT_MODE, SHIFT_LEFT, offsetof( COIL_DATA, servo_left_pos ) },
  {"Coil Width (us)", EDIT_MODE, SHIFT_RIGHT, offsetof( COIL_DATA, width_us ) },
  {"Done Editing", SELECT_MODE, EDITING, -1 }
};

const MenuItem mainMenu[] = {
  {"Wind Coil", SELECT_MODE, STARTING, -1 },
  {"Edit Coils", SELECT_MODE, EDITING, -1 },
  {"Save Data", MAIN_MODE, DONE, -1 },
  {"Hardware Setup", SETUP_MODE, STARTING, -1 },
  {"Reboot", MAIN_MODE, ABORTING, -1 }
};

const MenuItem pauseMenu[] = {
  //{"Paused...", PAUSED_MODE, STARTING, 0 },
  {"Resume Wind", WIND_MODE, RUNNING, 0 },
  {"Shift Left", WIND_MODE, SHIFT_LEFT, 0 },
  {"Shift Right", WIND_MODE, SHIFT_RIGHT, 0 },
  {"Abort Wind", WIND_MODE, DONE, 0 }
};

const MenuItem setupMenu[] = {
  {"Spindle Steps/Rev", SETUP_MODE, RUNNING, offsetof( HW_CONFIG, steps_per_rev ) },
  {"Smooth Servo Moves", SETUP_MODE, RUNNING, offsetof( HW_CONFIG, smooth_servo ) },
  {"RESET ALL DATA\nDANGER!", SETUP_MODE, ABORTING, -1 },
  {"Return to Main", MAIN_MODE, STARTING, -1 }
};

Menu menus[] = {
  { mainMenu, sizeof( mainMenu ) / sizeof(mainMenu[0]), NULL },
  { setupMenu, sizeof( setupMenu ) /sizeof(setupMenu[0]), &config },
  { pauseMenu, sizeof( pauseMenu ) /sizeof(pauseMenu[0]), &wind_count },
  { coilMenu, sizeof( coilMenu ) / sizeof( coilMenu[0] ), NULL },
  { coilEditMenu, sizeof( coilEditMenu ) / sizeof( coilEditMenu[0] ), NULL },
  { NULL, 0, NULL }
};

#endif
