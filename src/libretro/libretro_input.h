#ifndef LIBRETRO_INPUT_H
#define LIBRETRO_INPUT_H

#include "libretro.h"
#include "System.h"

typedef struct __HWSENSOR
{
   int tilt_x;
   int tilt_y;
   int gyro;
   int solar_level;
} HWSENSOR;

typedef struct __RETRO_BIND_MAP
{
   int retro;
   int gba;
} RETRO_BIND_MAP;

void update_input_descriptors(EmulatedSystem*, retro_environment_t);

void update_input_solar_sensor(int);

void update_input_rumble_state(bool);

void update_input(bool, retro_input_state_t);

// hardware sensors
int get_sensor_tilt_x();
int get_sensor_tilt_y();
int get_sensor_gyro();
int get_sensor_solar();

extern bool option_turboEnable;
extern unsigned option_turboDelay;
extern bool option_turboEnable;
extern unsigned option_turboDelay;
extern int option_analogDeadzone;
extern int option_gyroSensitivity;
extern int option_tiltSensitivity;
extern bool option_swapAnalogSticks;

extern uint16_t input_buf[4];
extern unsigned input_type[4];

#endif // LIBRETRO_INPUT_H