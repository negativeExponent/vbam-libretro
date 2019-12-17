#include <math.h>

#include "libretro_input.h"
#include "Util.h"

// GB
#include "gbGlobals.h"
#include "gbSGB.h"

#define BIT_LEFT(x)   (1 << (x))

#define ROUND(x)      floor((x) + 0.5)
#define ASTICK_MAX    0x8000

#define MAX_PLAYERS   4
#define MAX_BUTTONS   10
#define TURBO_BUTTONS 2

static retro_environment_t environ_cb     = NULL;
static retro_set_rumble_state_t rumble_cb = NULL;
static EmulatedSystem *core               = NULL;

static int sensorDarknessLevel;
static int analog_input_x;
static int analog_input_y;
static int analog_input_z;

static HWSENSOR sensor;

// delay in frames to turn off rumble effect since last enabled
static unsigned rumble_offtimer;

static unsigned turbo_delay_counter[MAX_PLAYERS][MAX_BUTTONS] = {};

// button inputs
static RETRO_BIND_MAP bindmap[MAX_BUTTONS] = {
   { RETRO_DEVICE_ID_JOYPAD_A,      JOY_A },
   { RETRO_DEVICE_ID_JOYPAD_B,      JOY_B },
   { RETRO_DEVICE_ID_JOYPAD_SELECT, JOY_SELECT },
   { RETRO_DEVICE_ID_JOYPAD_START,  JOY_START },
   { RETRO_DEVICE_ID_JOYPAD_RIGHT,  JOY_RIGHT },
   { RETRO_DEVICE_ID_JOYPAD_LEFT,   JOY_LEFT },
   { RETRO_DEVICE_ID_JOYPAD_UP,     JOY_UP },
   { RETRO_DEVICE_ID_JOYPAD_DOWN,   JOY_DOWN },
   { RETRO_DEVICE_ID_JOYPAD_R,      JOY_R },
   { RETRO_DEVICE_ID_JOYPAD_L,      JOY_L }
};

static const unsigned turbo_binds[TURBO_BUTTONS] = {
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_Y
};

static void update_input_motion_sensors()
{
   // Max ranges as set in VBAM
   static const int tilt_max    = 2197;
   static const int tilt_min    = 1897;
   static const int tilt_center = 2047;
   static const int gyro_thresh = 1800;

   if (sensor.tilt_x == 0)
      sensor.tilt_x = tilt_center;
   else
      sensor.tilt_x = (-analog_input_x) + tilt_center;

   if (sensor.tilt_x > tilt_max)
      sensor.tilt_x = tilt_max;
   else if (sensor.tilt_x < tilt_min)
      sensor.tilt_x = tilt_min;

   if (sensor.tilt_y == 0)
      sensor.tilt_y = tilt_center;
   else
      sensor.tilt_y = analog_input_y + tilt_center;

   if (sensor.tilt_y > tilt_max)
      sensor.tilt_y = tilt_max;
   else if (sensor.tilt_y < tilt_min)
      sensor.tilt_y = tilt_min;

   sensor.gyro = analog_input_z;
   if (sensor.gyro > gyro_thresh)
      sensor.gyro = gyro_thresh;
   else if (sensor.gyro < -gyro_thresh)
      sensor.gyro = -gyro_thresh;
}

static void update_input_tilt(int16_t axis_x, int16_t axis_y)
{
   // Convert cartesian coordinate analog stick to polar coordinates
   double radius = sqrt(axis_x * axis_x + axis_y * axis_y);
   double angle  = atan2(axis_y, axis_x);

   if (radius > option_analogDeadzone)
   {
      // Re-scale analog stick range to negate deadzone (makes slow movements possible)
      radius = (radius - option_analogDeadzone) * ((float)ASTICK_MAX / (ASTICK_MAX - option_analogDeadzone));
      // Tilt sensor range is from  from 1897 to 2197
      radius *= 150.0 / ASTICK_MAX * (option_tiltSensitivity / 100.0);
      // Convert back to cartesian coordinates
      analog_input_x = +(int16_t)ROUND(radius * cos(angle));
      analog_input_y = -(int16_t)ROUND(radius * sin(angle));
   }
   else
      analog_input_x = analog_input_y = 0;
}

static void update_input_gyro(int16_t axis_z)
{
   double scaled_range = 0.0;

   if (axis_z < -option_analogDeadzone)
   {
      // Re-scale analog stick range
      scaled_range = (-axis_z - option_analogDeadzone) * ((float)ASTICK_MAX / (ASTICK_MAX - option_analogDeadzone));
      // Gyro sensor range is +/- 1800
      scaled_range *= 1800.0 / ASTICK_MAX * (option_gyroSensitivity / 100.0);
      analog_input_z = -(int16_t)ROUND(scaled_range);
   }
   else if (axis_z > option_analogDeadzone)
   {
      scaled_range = (axis_z - option_analogDeadzone) * ((float)ASTICK_MAX / (ASTICK_MAX - option_analogDeadzone));
      scaled_range *= (1800.0 / ASTICK_MAX * (option_gyroSensitivity / 100.0));
      analog_input_z = +(int16_t)ROUND(scaled_range);
   }
   else
      analog_input_z = 0;
}

static void update_input_rumble()
{
   if (!rumble_cb)
      return;

   int rumble = rumble_offtimer ? 0xFFFF : 0;

   rumble_cb(0, RETRO_RUMBLE_STRONG, rumble);
   rumble_cb(0, RETRO_RUMBLE_WEAK, rumble);

   if (rumble_offtimer)
      --rumble_offtimer;
}

static int get_max_players()
{
   int ret = 1;

   if (core->type == IMAGE_GB)
   {
      if (gbSgbMode && gbSgbMultiplayer)
      {
         ret = 2;
         if (gbSgbFourPlayers)
            ret = 4;
      }
   }
   return ret;
}

// Tilt sensor X
int get_sensor_tilt_x()
{
   return sensor.tilt_x;
}

// Tilt sensor Y
int get_sensor_tilt_y()
{
   return sensor.tilt_y;
}

// GBA Gyro
int get_sensor_gyro()
{
   return sensor.gyro;
}

// GBA Solar Sensor
int get_sensor_solar()
{
   return sensor.solar_level;
}

void update_input_solar_sensor(int v)
{
   int value = 0;
   switch (v)
   {
   case 1: value = 0x06; break;
   case 2: value = 0x0E; break;
   case 3: value = 0x18; break;
   case 4: value = 0x20; break;
   case 5: value = 0x28; break;
   case 6: value = 0x38; break;
   case 7: value = 0x48; break;
   case 8: value = 0x60; break;
   case 9: value = 0x78; break;
   case 10: value = 0x98; break;
   default: break;
   }

   sensor.solar_level = 0xE8 - value;
}

void update_input_rumble_state(bool val)
{
   // if rumble is requested, reset timer
   if (val)
      rumble_offtimer = 5; // delay for 4 frames
   // TODO: Update rumble strength based on how many ON request
}

void update_input(bool libretro_supports_bitmasks, retro_input_state_t input_cb)
{
   unsigned max_players = get_max_players();

   bool sensor_button_down[2] = { false, false };

   for (unsigned port = 0; port < max_players; port++)
   {
      input_buf[port] = 0;

      // we only support JOYPAD input types
      if (input_type[port] == RETRO_DEVICE_JOYPAD)
      {
         bool turbo_button_down[2] = { false, false };

         // read all inputs once using input bitmasks
         if (libretro_supports_bitmasks)
         {
            int16_t ret = input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

            for (unsigned i = 0; i < MAX_BUTTONS; ++i)
            {
               if (ret & BIT_LEFT(bindmap[i].retro))
                  input_buf[port] |= bindmap[i].gba;
            }

            if (option_turboEnable)
               for (unsigned i = 0; i < TURBO_BUTTONS; ++i)
                  turbo_button_down[i] = (ret & BIT_LEFT(turbo_binds[i]));

            if (port == 0 && hardware & HW_SOLAR_SENSOR)
               for (unsigned i = 0; i < 2; ++i)
                  sensor_button_down[i] = (ret & BIT_LEFT(RETRO_DEVICE_ID_JOYPAD_L2 + i)) ? true : false;
         }

         // if not using input bitmask, buffer all buttons the standard way
         else
         {
            for (unsigned i = 0; i < MAX_BUTTONS; ++i)
               input_buf[port] |= input_cb(port, RETRO_DEVICE_JOYPAD, 0, bindmap[i].retro) ? bindmap[i].gba : 0;

            if (option_turboEnable)
               for (unsigned i = 0; i < TURBO_BUTTONS; ++i)
                  turbo_button_down[i] = input_cb(port, RETRO_DEVICE_JOYPAD, 0, turbo_binds[i]) ? true : false;

            if (port == 0 && hardware & HW_SOLAR_SENSOR)
               for (unsigned i = 0; i < 2; ++i)
                  sensor_button_down[i] = input_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2 + i) ? true : false;
         }

         // handle turbo button mode
         if (option_turboEnable)
         {
            // Handle Turbo A & B buttons
            for (unsigned j = 0; j < TURBO_BUTTONS; j++)
            {
               if (turbo_button_down[j] == true)
               {
                  // only trigger when counter is zero
                  if (turbo_delay_counter[port][j] == 0)
                     // confirm button state
                     input_buf[port] |= (JOY_A + j);
                  else
                     // remove button state
                     input_buf[port] &= ~(JOY_A + j);

                  // Reset the toggle if delay value is reached
                  if (++turbo_delay_counter[port][j] > option_turboDelay)
                     turbo_delay_counter[port][j] = 0;
               }
               else
                  // just reset pressed state and counter if no turbo buttons were pressed
                  turbo_delay_counter[port][j] = 0;
            }
         }
      }

      // Do not allow opposing directions
      if ((input_buf[port] & (JOY_UP | JOY_DOWN)) == (JOY_UP | JOY_DOWN))
         input_buf[port] &= ~(JOY_UP | JOY_DOWN);
      else if ((input_buf[port] & (JOY_LEFT | JOY_RIGHT)) == (JOY_LEFT | JOY_RIGHT))
         input_buf[port] &= ~(JOY_LEFT | JOY_RIGHT);
   }

   // Process hardware motion sensor inputs

   if (hardware & (HW_TILT | HW_GYRO))
   {
      unsigned tilt_index = option_swapAnalogSticks ? RETRO_DEVICE_INDEX_ANALOG_LEFT : RETRO_DEVICE_INDEX_ANALOG_RIGHT;
      unsigned gyro_index = option_swapAnalogSticks ? RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

      if (hardware & HW_TILT)
      {
         int16_t axis_x = input_cb(0, RETRO_DEVICE_ANALOG, tilt_index, RETRO_DEVICE_ID_ANALOG_X);
         int16_t axis_y = input_cb(0, RETRO_DEVICE_ANALOG, tilt_index, RETRO_DEVICE_ID_ANALOG_Y);

         update_input_tilt(axis_x, axis_y);
      }

      else if (hardware & HW_GYRO)
      {
         int16_t axis_z = input_cb(0, RETRO_DEVICE_ANALOG, gyro_index, RETRO_DEVICE_ID_ANALOG_X);

         update_input_gyro(axis_z);
      }

      update_input_motion_sensors();
   }

   else if (hardware & HW_SOLAR_SENSOR)
   {
      static bool buttonpressed = false;

      if (sensor_button_down[1] == true)
      {
         if (buttonpressed == false)
         {
            buttonpressed = true;
            if (++sensorDarknessLevel > 10)
               sensorDarknessLevel = 10;
            update_input_solar_sensor(sensorDarknessLevel);
         }
      }
      else if (sensor_button_down[0] == true)
      {
         if (buttonpressed == false)
         {
            buttonpressed = true;
            if (--sensorDarknessLevel < 0)
               sensorDarknessLevel = 0;
            update_input_solar_sensor(sensorDarknessLevel);
         }
      }
      else
         buttonpressed = false;
   }

   if (hardware & HW_RUMBLE)
      update_input_rumble();
}

void update_input_descriptors(EmulatedSystem *EmuSys, retro_environment_t EvCB)
{
#define INPUT_JOY(port)                                                            \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "D-Pad Left" },  \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "D-Pad Up" },    \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "D-Pad Down" },  \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "D-Pad Right" }, \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },           \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },           \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Turbo A" },     \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Turbo B" },     \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },      \
   { port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" }

#define INPUT_JOY_W_ANALOG(port)                                                              \
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Tilt Sensor X" },  \
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Tilt Sensor Y" },  \
   { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Right - Y-Axis" },  \
   INPUT_JOY(port)

   static struct retro_input_descriptor input_gba[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,  "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,  "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Solar Sensor (Darker)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Solar Sensor (Lighter)" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Tilt Sensor X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Tilt Sensor Y" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Gyro Sensor" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog - Y-Axis" },
      INPUT_JOY(0),
      { 0, 0, 0, 0, NULL },
   };

   static struct retro_input_descriptor input_gb[] = {
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Tilt Sensor X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Tilt Sensor Y" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Analog X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog Y" },
      INPUT_JOY(0),
      INPUT_JOY(1),
      INPUT_JOY(2),
      INPUT_JOY(3),
      { 0, 0, 0, 0, NULL },
   };

   static const struct retro_controller_description port_gba[] = {
      { "GBA Joypad", RETRO_DEVICE_JOYPAD },
      { NULL, 0 },
   };

   static const struct retro_controller_description port_gb[] = {
      { "GB Joypad", RETRO_DEVICE_JOYPAD },
      { NULL, 0 },
   };

   static const struct retro_controller_info ports_gba[] = {
      { port_gba, 1 },
      { NULL,     0 }
   };

   static const struct retro_controller_info ports_gb[] = {
      { port_gb, 1 },
      { port_gb, 1 },
      { port_gb, 1 },
      { port_gb, 1 },
      { NULL,    0 }
   };

   environ_cb = EvCB;
   core       = EmuSys;

   if (core->type == IMAGE_GBA)
   {
      environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,   (void *)ports_gba);
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gba);
   }

   else if (core->type == IMAGE_GB)
   {
      environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,   (void *)ports_gb);
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gb);
   }

   struct retro_rumble_interface rumble;
   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
      rumble_cb = rumble.set_rumble_state;
}
