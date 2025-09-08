#ifndef _Types_h
#define _Types_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

typedef struct
{
	char *description;
	int16_t new_mode;
	int16_t new_state;
	int16_t value_offset;
} MenuItem;

typedef struct
{
	MenuItem *items;
	unsigned num_items;
	void *value_ptr;
} Menu;

typedef struct {
  uint8_t   version;
  uint16_t  steps_per_rev;
  uint16_t  smooth_servo;
} HW_CONFIG;

typedef struct
{
  uint16_t  inner_diameter;
  uint16_t  width_mm;
  uint16_t  wire_type;
  uint16_t  ohms;
  uint16_t  spindle_rpm;
  uint16_t  num_winds;
  uint16_t  winds_per_layer;
  uint16_t  servo_left_pos;
  uint16_t  width_us;
} COIL_DATA;

typedef enum { MAIN_MODE, SETUP_MODE, PAUSED_MODE, SELECT_MODE, EDIT_MODE, WIND_MODE } MODE;
typedef enum  { STARTING, RUNNING, SHIFT_LEFT, SHIFT_RIGHT, PAUSED, ABORTING, DONE, EDITING } STATE;

#endif
