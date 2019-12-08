#include <string.h>

#include "../Util.h"
#include "../gba/Sound.h"
#include "gb.h"
#include "gbGlobals.h"
#include "gbSound.h"

#include "../apu/Effects_Buffer.h"
#include "../apu/Gb_Apu.h"

extern long soundSampleRate; // current sound quality

gb_effects_config_t gb_effects_config = { false, 0.20f, 0.15f, false };

static gb_effects_config_t gb_effects_config_current;
static Simple_Effects_Buffer* stereo_buffer;
static Gb_Apu* gb_apu;

static float soundVolume_ = -1;
static int prevSoundEnable = -1;
static bool declicking = false;

static int const chan_count = 4;
static int const ticks_to_time = 2 * GB_APU_OVERCLOCK;
static int soundTicksUp = 0;

void gbUpdateSoundTicks(int ticks)
{
    soundTicksUp += ticks;
}

static inline blip_time_t blip_time()
{
    return soundTicksUp * ticks_to_time;
}

uint8_t gbSoundRead(uint16_t address)
{
    return gb_apu->read_register(blip_time(), address);
}

void gbSoundEvent(uint16_t address, int data)
{
    gbMemory[address] = data;
    gb_apu->write_register(blip_time(), address, data);
}

static void end_frame(blip_time_t time)
{
    gb_apu->end_frame(time);
    stereo_buffer->end_frame(time);
}

static void apply_effects()
{
    prevSoundEnable = soundGetEnable();
    gb_effects_config_current = gb_effects_config;

    stereo_buffer->config().enabled = gb_effects_config_current.enabled;
    stereo_buffer->config().echo = gb_effects_config_current.echo;
    stereo_buffer->config().stereo = gb_effects_config_current.stereo;
    stereo_buffer->config().surround = gb_effects_config_current.surround;
    stereo_buffer->apply_config();

    for (int i = 0; i < chan_count; i++) {
        Multi_Buffer::channel_t ch = { 0, 0, 0 };
        if (prevSoundEnable >> i & 1)
            ch = stereo_buffer->channel(i);
        gb_apu->set_output(ch.center, ch.left, ch.right, i);
    }
}

void gbSoundConfigEffects(gb_effects_config_t const& c)
{
    gb_effects_config = c;
}

static void apply_volume()
{
    soundVolume_ = soundGetVolume();

    if (gb_apu)
        gb_apu->volume(soundVolume_);
}

void gbSoundTick()
{
    if (gb_apu && stereo_buffer) {
        // Run sound hardware to present
        end_frame(blip_time());

        flush_samples(stereo_buffer);

        // Update effects config if it was changed
        if (memcmp(&gb_effects_config_current, &gb_effects_config,
                sizeof gb_effects_config)
            || soundGetEnable() != prevSoundEnable)
            apply_effects();

        if (soundVolume_ != soundGetVolume())
            apply_volume();
    }

    soundTicksUp = 0;
}

int gbSoundEndFrame(int16_t *soundbuf)
{
   int numSamples = 0;
   if (soundbuf && gb_apu && stereo_buffer)
   {
      blip_time_t ticks = blip_time();
      gb_apu->end_frame(ticks);
      stereo_buffer->end_frame(ticks);
      numSamples = stereo_buffer->read_samples((blip_sample_t*)soundbuf, stereo_buffer->samples_avail());

      // Update effects config if it was changed
      if (memcmp(&gb_effects_config_current, &gb_effects_config,
           sizeof gb_effects_config) ||
           soundGetEnable() != prevSoundEnable)
      {
         apply_effects();
      }
      if (soundVolume_ != soundGetVolume())
      {
         apply_volume();
      }
   }
   soundTicksUp = 0;
   return numSamples;
}

static void reset_apu()
{
    Gb_Apu::mode_t mode = Gb_Apu::mode_dmg;
    if (gbHardware & 2)
        mode = Gb_Apu::mode_cgb;
    if (gbHardware & 8 || declicking)
        mode = Gb_Apu::mode_agb;
    gb_apu->reset(mode);
    gb_apu->reduce_clicks(declicking);

    if (stereo_buffer)
        stereo_buffer->clear();

    soundTicksUp = 0;
}

static void remake_stereo_buffer()
{
    // APU
    if (!gb_apu) {
        gb_apu = new Gb_Apu; // TODO: handle errors
        reset_apu();
    }

    // Stereo_Buffer
    delete stereo_buffer;
    stereo_buffer = 0;

    stereo_buffer = new Simple_Effects_Buffer; // TODO: handle out of memory
    if (stereo_buffer->set_sample_rate(soundSampleRate)) {
    } // TODO: handle out of memory
    stereo_buffer->clock_rate(gb_apu->clock_rate);

    // Multi_Buffer
    static int const chan_types[chan_count] = {
        Multi_Buffer::wave_type + 1, Multi_Buffer::wave_type + 2,
        Multi_Buffer::wave_type + 3, Multi_Buffer::mixed_type + 1
    };
    if (stereo_buffer->set_channel_count(chan_count, chan_types)) {
    } // TODO: handle errors

    // Volume Level
    apply_effects();
    apply_volume();
}

void gbSoundSetDeclicking(bool enable)
{
    if (declicking != enable) {
        declicking = enable;
        if (gb_apu) {
            // Can't change sound hardware mode without resetting APU, so save/load
            // state around mode change
            gb_apu_state_t state;
            gb_apu->save_state(&state);
            reset_apu();
            if (gb_apu->load_state(state)) {
            } // ignore error
        }
    }
}

bool gbSoundGetDeclicking()
{
    return declicking;
}

void gbSoundReset()
{
    remake_stereo_buffer();
    reset_apu();

    soundPaused = 1;

    gbSoundEvent(0xff10, 0x80);
    gbSoundEvent(0xff11, 0xbf);
    gbSoundEvent(0xff12, 0xf3);
    gbSoundEvent(0xff14, 0xbf);
    gbSoundEvent(0xff16, 0x3f);
    gbSoundEvent(0xff17, 0x00);
    gbSoundEvent(0xff19, 0xbf);

    gbSoundEvent(0xff1a, 0x7f);
    gbSoundEvent(0xff1b, 0xff);
    gbSoundEvent(0xff1c, 0xbf);
    gbSoundEvent(0xff1e, 0xbf);

    gbSoundEvent(0xff20, 0xff);
    gbSoundEvent(0xff21, 0x00);
    gbSoundEvent(0xff22, 0x00);
    gbSoundEvent(0xff23, 0xbf);
    gbSoundEvent(0xff24, 0x77);
    gbSoundEvent(0xff25, 0xf3);

    if (gbHardware & 0x4)
        gbSoundEvent(0xff26, 0xf0);
    else
        gbSoundEvent(0xff26, 0xf1);

    /* workaround for game Beetlejuice */
    if (gbHardware & 0x1) {
        gbSoundEvent(0xff24, 0x77);
        gbSoundEvent(0xff25, 0xf3);
    }

    int addr = 0xff30;

    while (addr < 0xff40) {
        gbMemory[addr++] = 0x00;
        gbMemory[addr++] = 0xff;
    }
}

void gbSoundSetSampleRate(long sampleRate)
{
    if (soundSampleRate != sampleRate) {
        if (systemCanChangeSoundQuality()) {
            soundShutdown();
            soundSampleRate = sampleRate;
            soundInit();
        } else {
            soundSampleRate = sampleRate;
        }

        remake_stereo_buffer();
    }
}

static struct {
    int version;
    gb_apu_state_t apu;
} state;

static char dummy_state[735 * 2];

#define SKIP(type, name)          \
    {                             \
        dummy_state, sizeof(type) \
    }

#define LOAD(type, name)    \
    {                       \
        &name, sizeof(type) \
    }

// New state format
static variable_desc gb_state[] = {
    LOAD(int, state.version), // room_for_expansion will be used by later versions

    // APU
    LOAD(uint8_t[0x40], state.apu.regs), // last values written to registers and wave RAM (both banks)
    LOAD(int, state.apu.frame_time), // clocks until next frame sequencer action
    LOAD(int, state.apu.frame_phase), // next step frame sequencer will run

    LOAD(int, state.apu.sweep_freq), // sweep's internal frequency register
    LOAD(int, state.apu.sweep_delay), // clocks until next sweep action
    LOAD(int, state.apu.sweep_enabled),
    LOAD(int, state.apu.sweep_neg), // obscure internal flag
    LOAD(int, state.apu.noise_divider),
    LOAD(int, state.apu.wave_buf), // last read byte of wave RAM

    LOAD(int[4], state.apu.delay), // clocks until next channel action
    LOAD(int[4], state.apu.length_ctr),
    LOAD(int[4], state.apu.phase), // square/wave phase, noise LFSR
    LOAD(int[4], state.apu.enabled), // internal enabled flag

    LOAD(int[3], state.apu.env_delay), // clocks until next envelope action
    LOAD(int[3], state.apu.env_volume),
    LOAD(int[3], state.apu.env_enabled),

    SKIP(int[13], room_for_expansion),

    // Emulator
    LOAD(int, soundTicksUp),
    SKIP(int[15], room_for_expansion),

    { NULL, 0 }
};

void gbSoundSaveGame(uint8_t*& out)
{
    gb_apu->save_state(&state.apu);

    // Be sure areas for expansion get written as zero
    memset(dummy_state, 0, sizeof dummy_state);

    state.version = 1;
    utilWriteDataMem(out, gb_state);
}

void gbSoundReadGame(const uint8_t*& in, int version)
{
    // Prepare APU and default state
    reset_apu();
    gb_apu->save_state(&state.apu);
    utilReadDataMem(in, gb_state);
    gb_apu->load_state(state.apu);
}
