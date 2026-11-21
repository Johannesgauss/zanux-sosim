//
// Freestanding Hardware OPL2/OPL3 MUS Music Driver for Doom on Zanux OS
//
// Implements the music_module_t interface from i_sound.h:
// - Parses Doom's native GENMIDI lump for OPL instrument patch parameters.
// - Plays Doom MUS format music directly via hardware OPL port 0x388 (AdLib/SB).
// - Polled at 140 Hz using wall-clock ms timing (I_GetTimeMS).
//

#include "doomtype.h"
#include "i_sound.h"
#include "i_timer.h"
#include "w_wad.h"
#include "z_zone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.h"

// -----------------------------------------------------------------------------
// OPL Hardware Register & Port Definitions
// -----------------------------------------------------------------------------

#define OPL_PORT_BASE               0x388

#define OPL_NUM_VOICES_OPL2         9
#define OPL_NUM_VOICES_OPL3         18
#define OPL_MAX_VOICES              18

#define OPL_REG_WAVEFORM_ENABLE     0x01
#define OPL_REG_TIMER1              0x02
#define OPL_REG_TIMER2              0x03
#define OPL_REG_TIMER_CTRL          0x04
#define OPL_REG_FM_MODE             0x08
#define OPL_REG_OPL3_NEW            0x105

#define OPL_REGS_TREMOLO            0x20
#define OPL_REGS_LEVEL              0x40
#define OPL_REGS_ATTACK             0x60
#define OPL_REGS_SUSTAIN            0x80
#define OPL_REGS_FREQ_1             0xA0
#define OPL_REGS_FREQ_2             0xB0
#define OPL_REGS_FEEDBACK           0xC0
#define OPL_REGS_WAVEFORM           0xE0

// -----------------------------------------------------------------------------
// Low-Level Port I/O
// Using outb(0x388, reg); inb(0x388); outb(0x389, val); inb(0x388);
// -----------------------------------------------------------------------------

static inline void OPL_PortWrite(uint16_t port, uint8_t val)
{
    sys_opl_write(port, val);
}

static inline uint8_t OPL_PortRead(uint16_t port)
{
    return sys_opl_read(port);
}

static void OPL_WriteRegister(int reg, int val)
{
    uint16_t base = OPL_PORT_BASE;
    uint16_t reg_port = (reg & 0x100) ? (base + 2) : base;
    uint16_t data_port = reg_port + 1;

    OPL_PortWrite(reg_port, (uint8_t)(reg & 0xff));
    OPL_PortRead(OPL_PORT_BASE);
    OPL_PortWrite(data_port, (uint8_t)val);
    OPL_PortRead(OPL_PORT_BASE);
}

// -----------------------------------------------------------------------------
// GENMIDI Structures (OPL Instrument Patches)
// -----------------------------------------------------------------------------

#define GENMIDI_NUM_INSTRS          128
#define GENMIDI_NUM_PERCUSSION      47
#define GENMIDI_FLAG_FIXED          0x0001
#define GENMIDI_FLAG_2VOICE         0x0004

typedef struct __attribute__((packed))
{
    uint8_t tremolo;
    uint8_t attack;
    uint8_t sustain;
    uint8_t waveform;
    uint8_t scale;
    uint8_t level;
} genmidi_op_t;

typedef struct __attribute__((packed))
{
    genmidi_op_t modulator;
    uint8_t feedback;
    genmidi_op_t carrier;
    uint8_t unused;
    int16_t base_note_offset;
} genmidi_voice_t;

typedef struct __attribute__((packed))
{
    uint16_t flags;
    uint8_t fine_tuning;
    uint8_t fixed_note;
    genmidi_voice_t voices[2];
} genmidi_instr_t;

// -----------------------------------------------------------------------------
// OPL Voice and Channel State
// -----------------------------------------------------------------------------

typedef struct opl_voice_s opl_voice_t;

typedef struct
{
    genmidi_instr_t *instrument;
    int volume;
    int volume_base;
    int pan;
    int bend;
    uint8_t last_velocity;
} opl_channel_data_t;

struct opl_voice_s
{
    int index;
    int op1;
    int op2;
    int array;

    genmidi_instr_t *current_instr;
    unsigned int current_instr_voice;

    opl_channel_data_t *channel;
    unsigned int key;
    unsigned int note;
    unsigned int freq;

    unsigned int note_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    unsigned int reg_pan;
    unsigned int priority;
};

// Operator register offsets for voices 0..8
static const int voice_operators[2][9] = {
    { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 },
    { 0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15 }
};

// -----------------------------------------------------------------------------
// Frequency Curve Table
// -----------------------------------------------------------------------------

static const unsigned short frequency_curve[] = {
    0x133, 0x133, 0x134, 0x134, 0x135, 0x136, 0x136, 0x137,
    0x137, 0x138, 0x138, 0x139, 0x139, 0x13a, 0x13b, 0x13b,
    0x13c, 0x13c, 0x13d, 0x13d, 0x13e, 0x13f, 0x13f, 0x140,
    0x140, 0x141, 0x142, 0x142, 0x143, 0x143, 0x144, 0x144,

    0x145, 0x146, 0x146, 0x147, 0x147, 0x148, 0x149, 0x149,
    0x14a, 0x14a, 0x14b, 0x14c, 0x14c, 0x14d, 0x14d, 0x14e,
    0x14f, 0x14f, 0x150, 0x150, 0x151, 0x152, 0x152, 0x153,
    0x153, 0x154, 0x155, 0x155, 0x156, 0x157, 0x157, 0x158,

    0x158, 0x159, 0x15a, 0x15a, 0x15b, 0x15b, 0x15c, 0x15d,
    0x15d, 0x15e, 0x15f, 0x15f, 0x160, 0x161, 0x161, 0x162,
    0x162, 0x163, 0x164, 0x164, 0x165, 0x166, 0x166, 0x167,
    0x168, 0x168, 0x169, 0x16a, 0x16a, 0x16b, 0x16c, 0x16c,

    0x16d, 0x16e, 0x16e, 0x16f, 0x170, 0x170, 0x171, 0x172,
    0x172, 0x173, 0x174, 0x174, 0x175, 0x176, 0x176, 0x177,
    0x178, 0x178, 0x179, 0x17a, 0x17a, 0x17b, 0x17c, 0x17c,
    0x17d, 0x17e, 0x17e, 0x17f, 0x180, 0x181, 0x181, 0x182,

    0x183, 0x183, 0x184, 0x185, 0x185, 0x186, 0x187, 0x188,
    0x188, 0x189, 0x18a, 0x18a, 0x18b, 0x18c, 0x18d, 0x18d,
    0x18e, 0x18f, 0x18f, 0x190, 0x191, 0x192, 0x192, 0x193,
    0x194, 0x194, 0x195, 0x196, 0x197, 0x197, 0x198, 0x199,

    0x19a, 0x19a, 0x19b, 0x19c, 0x19d, 0x19d, 0x19e, 0x19f,
    0x1a0, 0x1a0, 0x1a1, 0x1a2, 0x1a3, 0x1a3, 0x1a4, 0x1a5,
    0x1a6, 0x1a6, 0x1a7, 0x1a8, 0x1a9, 0x1a9, 0x1aa, 0x1ab,
    0x1ac, 0x1ad, 0x1ad, 0x1ae, 0x1af, 0x1b0, 0x1b0, 0x1b1,

    0x1b2, 0x1b3, 0x1b4, 0x1b4, 0x1b5, 0x1b6, 0x1b7, 0x1b8,
    0x1b8, 0x1b9, 0x1ba, 0x1bb, 0x1bc, 0x1bc, 0x1bd, 0x1be,
    0x1bf, 0x1c0, 0x1c0, 0x1c1, 0x1c2, 0x1c3, 0x1c4, 0x1c4,
    0x1c5, 0x1c6, 0x1c7, 0x1c8, 0x1c9, 0x1c9, 0x1ca, 0x1cb,

    0x1cc, 0x1cd, 0x1ce, 0x1ce, 0x1cf, 0x1d0, 0x1d1, 0x1d2,
    0x1d3, 0x1d3, 0x1d4, 0x1d5, 0x1d6, 0x1d7, 0x1d8, 0x1d8,
    0x1d9, 0x1da, 0x1db, 0x1dc, 0x1dd, 0x1de, 0x1de, 0x1df,
    0x1e0, 0x1e1, 0x1e2, 0x1e3, 0x1e4, 0x1e5, 0x1e5, 0x1e6,

    0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed, 0x1ed,
    0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5,
    0x1f6, 0x1f6, 0x1f7, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc,
    0x1fd, 0x1fe, 0x1ff, 0x200, 0x201, 0x201, 0x202, 0x203,

    0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x20a, 0x20b,
    0x20c, 0x20d, 0x20e, 0x20f, 0x210, 0x210, 0x211, 0x212,
    0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21a,
    0x21b, 0x21c, 0x21d, 0x21e, 0x21f, 0x220, 0x221, 0x222,

    0x223, 0x224, 0x225, 0x226, 0x227, 0x228, 0x229, 0x22a,
    0x22b, 0x22c, 0x22d, 0x22e, 0x22f, 0x230, 0x231, 0x232,
    0x233, 0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23a,
    0x23b, 0x23c, 0x23d, 0x23e, 0x23f, 0x240, 0x241, 0x242,

    0x244, 0x245, 0x246, 0x247, 0x248, 0x249, 0x24a, 0x24b,
    0x24c, 0x24d, 0x24e, 0x24f, 0x250, 0x251, 0x252, 0x253,
    0x254, 0x256, 0x257, 0x258, 0x259, 0x25a, 0x25b, 0x25c,
    0x25d, 0x25e, 0x25f, 0x260, 0x262, 0x263, 0x264, 0x265,

    0x266, 0x267, 0x268, 0x269, 0x26a, 0x26c, 0x26d, 0x26e,
    0x26f, 0x270, 0x271, 0x272, 0x273, 0x275, 0x276, 0x277,
    0x278, 0x279, 0x27a, 0x27b, 0x27d, 0x27e, 0x27f, 0x280,
    0x281, 0x282, 0x284, 0x285, 0x286, 0x287, 0x288, 0x289,

    0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290, 0x292, 0x293,
    0x294, 0x295, 0x296, 0x298, 0x299, 0x29a, 0x29b, 0x29c,
    0x29e, 0x29f, 0x2a0, 0x2a1, 0x2a2, 0x2a4, 0x2a5, 0x2a6,
    0x2a7, 0x2a9, 0x2aa, 0x2ab, 0x2ac, 0x2ae, 0x2af, 0x2b0,

    0x2b1, 0x2b2, 0x2b4, 0x2b5, 0x2b6, 0x2b7, 0x2b9, 0x2ba,
    0x2bb, 0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c2, 0x2c3, 0x2c4,
    0x2c5, 0x2c7, 0x2c8, 0x2c9, 0x2cb, 0x2cc, 0x2cd, 0x2ce,
    0x2d0, 0x2d1, 0x2d2, 0x2d4, 0x2d5, 0x2d6, 0x2d8, 0x2d9,

    0x2da, 0x2dc, 0x2dd, 0x2de, 0x2e0, 0x2e1, 0x2e2, 0x2e4,
    0x2e5, 0x2e6, 0x2e8, 0x2e9, 0x2ea, 0x2ec, 0x2ed, 0x2ee,
    0x2f0, 0x2f1, 0x2f2, 0x2f4, 0x2f5, 0x2f6, 0x2f8, 0x2f9,
    0x2fb, 0x2fc, 0x2fd, 0x2ff, 0x300, 0x302, 0x303, 0x304,

    0x306, 0x307, 0x309, 0x30a, 0x30b, 0x30d, 0x30e, 0x310,
    0x311, 0x312, 0x314, 0x315, 0x317, 0x318, 0x31a, 0x31b,
    0x31c, 0x31e, 0x31f, 0x321, 0x322, 0x324, 0x325, 0x327,
    0x328, 0x329, 0x32b, 0x32c, 0x32e, 0x32f, 0x331, 0x332,

    0x334, 0x335, 0x337, 0x338, 0x33a, 0x33b, 0x33d, 0x33e,
    0x340, 0x341, 0x343, 0x344, 0x346, 0x347, 0x349, 0x34a,
    0x34c, 0x34d, 0x34f, 0x350, 0x352, 0x353, 0x355, 0x357,
    0x358, 0x35a, 0x35b, 0x35d, 0x35e, 0x360, 0x361, 0x363,

    0x365, 0x366, 0x368, 0x369, 0x36b, 0x36c, 0x36e, 0x370,
    0x371, 0x373, 0x374, 0x376, 0x378, 0x379, 0x37b, 0x37c,
    0x37e, 0x380, 0x381, 0x383, 0x384, 0x386, 0x388, 0x389,
    0x38b, 0x38d, 0x38e, 0x390, 0x392, 0x393, 0x395, 0x397,

    0x398, 0x39a, 0x39c, 0x39d, 0x39f, 0x3a1, 0x3a2, 0x3a4,
    0x3a6, 0x3a7, 0x3a9, 0x3ab, 0x3ac, 0x3ae, 0x3b0, 0x3b1,
    0x3b3, 0x3b5, 0x3b7, 0x3b8, 0x3ba, 0x3bc, 0x3bd, 0x3bf,
    0x3c1, 0x3c3, 0x3c4, 0x3c6, 0x3c8, 0x3ca, 0x3cb, 0x3cd,

    0x3cf, 0x3d1, 0x3d2, 0x3d4, 0x3d6, 0x3d8, 0x3da, 0x3db,
    0x3dd, 0x3df, 0x3e1, 0x3e3, 0x3e4, 0x3e6, 0x3e8, 0x3ea,
    0x3ec, 0x3ed, 0x3ef, 0x3f1, 0x3f3, 0x3f5, 0x3f6, 0x3f8,
    0x3fa, 0x3fc, 0x3fe, 0x36c
};

// -----------------------------------------------------------------------------
// Volume Mapping Table (MIDI 0..127 to OPL attenuation)
// -----------------------------------------------------------------------------

static const unsigned int volume_mapping_table[128] = {
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

// -----------------------------------------------------------------------------
// Driver State Variables
// -----------------------------------------------------------------------------

static bool opl_initialized = false;
static bool is_opl3 = false;
static int num_opl_voices = OPL_NUM_VOICES_OPL2;

static int current_music_volume = 127;
static bool music_playing = false;
static bool music_paused = false;

static genmidi_instr_t *main_instrs = NULL;
static genmidi_instr_t *percussion_instrs = NULL;

static opl_voice_t voices[OPL_MAX_VOICES];
static opl_voice_t *voice_free_list[OPL_MAX_VOICES];
static opl_voice_t *voice_alloced_list[OPL_MAX_VOICES];
static int voice_free_num = 0;
static int voice_alloced_num = 0;

static opl_channel_data_t channels[16];

// Song Representation
typedef struct
{
    const uint8_t *data;
    int len;
    uint16_t score_start;
    uint16_t score_len;
} mus_song_t;

static mus_song_t *current_song = NULL;
static const uint8_t *song_pos = NULL;
static const uint8_t *song_end = NULL;
static bool song_looping = false;
static uint32_t song_delay = 0;
static uint32_t last_poll_ms = 0;
static uint32_t accum_ticks = 0;

// Forward declarations
static void ReleaseVoice(int index);
static void KeyOff(int ch, int key);
static void KeyOn(int ch, int key, int volume);
static void AllNotesOff(opl_channel_data_t *channel);

// -----------------------------------------------------------------------------
// Voice Allocation & Freelist
// -----------------------------------------------------------------------------

static void InitVoices(void)
{
    voice_free_num = num_opl_voices;
    voice_alloced_num = 0;

    for (int i = 0; i < num_opl_voices; ++i)
    {
        voices[i].index = i % 9;
        voices[i].op1 = voice_operators[0][i % 9];
        voices[i].op2 = voice_operators[1][i % 9];
        voices[i].array = (i / 9) << 8;
        voices[i].current_instr = NULL;
        voices[i].current_instr_voice = 0;
        voices[i].channel = NULL;
        voices[i].key = 0;
        voices[i].note = 0;
        voices[i].freq = 0;
        voices[i].note_volume = 0;
        voices[i].car_volume = 0x3f;
        voices[i].mod_volume = 0x3f;
        voices[i].reg_pan = 0x30;
        voices[i].priority = 0;

        voice_free_list[i] = &voices[i];
        voice_alloced_list[i] = NULL;
    }
}

static opl_voice_t *GetFreeVoice(void)
{
    if (voice_free_num == 0)
        return NULL;

    opl_voice_t *v = voice_free_list[0];
    voice_free_num--;

    for (int i = 0; i < voice_free_num; i++)
    {
        voice_free_list[i] = voice_free_list[i + 1];
    }

    voice_alloced_list[voice_alloced_num++] = v;
    return v;
}

static void VoiceKeyOff(opl_voice_t *voice)
{
    // Clear key-on bit (bit 5) in register 0xB0 + index
    OPL_WriteRegister((OPL_REGS_FREQ_2 + voice->index) | voice->array, voice->freq >> 8);
}

static void ReleaseVoice(int index)
{
    if (index < 0 || index >= voice_alloced_num)
        return;

    opl_voice_t *voice = voice_alloced_list[index];
    VoiceKeyOff(voice);

    voice->channel = NULL;
    voice->note = 0;
    voice->key = 0;

    voice_alloced_num--;
    for (int i = index; i < voice_alloced_num; i++)
    {
        voice_alloced_list[i] = voice_alloced_list[i + 1];
    }

    voice_free_list[voice_free_num++] = voice;
}

// Discard an active voice when polyphony limit is reached (DMX priority stealing)
static void ReplaceExistingVoice(void)
{
    if (voice_alloced_num == 0)
        return;

    int result = 0;
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->current_instr_voice != 0 ||
            voice_alloced_list[i]->channel >= voice_alloced_list[result]->channel)
        {
            result = i;
        }
    }

    ReleaseVoice(result);
}

// -----------------------------------------------------------------------------
// OPL Voice Frequency & Volume Calculations
// -----------------------------------------------------------------------------

static unsigned int FrequencyForVoice(opl_voice_t *voice)
{
    genmidi_voice_t *gm_voice;
    int note = voice->note;

    gm_voice = &voice->current_instr->voices[voice->current_instr_voice];

    if ((voice->current_instr->flags & GENMIDI_FLAG_FIXED) == 0)
    {
        note += gm_voice->base_note_offset;
    }

    while (note < 0) note += 12;
    while (note > 95) note -= 12;

    int freq_index = 64 + 32 * note + voice->channel->bend;

    if (voice->current_instr_voice != 0)
    {
        freq_index += (voice->current_instr->fine_tuning / 2) - 64;
    }

    if (freq_index < 0) freq_index = 0;

    if (freq_index < 284)
    {
        return frequency_curve[freq_index];
    }

    unsigned int sub_index = (freq_index - 284) % (12 * 32);
    unsigned int octave = (freq_index - 284) / (12 * 32);

    if (octave >= 7) octave = 7;

    return frequency_curve[sub_index + 284] | (octave << 10);
}

static void UpdateVoiceFrequency(opl_voice_t *voice)
{
    unsigned int freq = FrequencyForVoice(voice);

    if (voice->freq != freq)
    {
        OPL_WriteRegister((OPL_REGS_FREQ_1 + voice->index) | voice->array,
                          freq & 0xff);
        // Turn key-on bit (0x20) ON
        OPL_WriteRegister((OPL_REGS_FREQ_2 + voice->index) | voice->array,
                          (freq >> 8) | 0x20);

        voice->freq = freq;
    }
}

static void LoadOperatorData(int op, genmidi_op_t *data, bool max_level, unsigned int *volume)
{
    int level = data->scale;

    if (max_level)
    {
        level |= 0x3f;
    }
    else
    {
        level |= data->level;
    }

    *volume = level;

    OPL_WriteRegister(OPL_REGS_LEVEL + op, level);
    OPL_WriteRegister(OPL_REGS_TREMOLO + op, data->tremolo);
    OPL_WriteRegister(OPL_REGS_ATTACK + op, data->attack);
    OPL_WriteRegister(OPL_REGS_SUSTAIN + op, data->sustain);
    OPL_WriteRegister(OPL_REGS_WAVEFORM + op, data->waveform);
}

static void SetVoiceInstrument(opl_voice_t *voice, genmidi_instr_t *instr, unsigned int instr_voice)
{
    if (voice->current_instr == instr && voice->current_instr_voice == instr_voice)
    {
        return;
    }

    voice->current_instr = instr;
    voice->current_instr_voice = instr_voice;

    genmidi_voice_t *data = &instr->voices[instr_voice];
    bool modulating = (data->feedback & 0x01) == 0;

    LoadOperatorData(voice->op2 | voice->array, &data->carrier, true, &voice->car_volume);
    LoadOperatorData(voice->op1 | voice->array, &data->modulator, !modulating, &voice->mod_volume);

    OPL_WriteRegister((OPL_REGS_FEEDBACK + voice->index) | voice->array,
                      data->feedback | voice->reg_pan);

    voice->priority = 0x0f - (data->carrier.attack >> 4)
                    + 0x0f - (data->carrier.sustain & 0x0f);
}

static void SetVoiceVolume(opl_voice_t *voice, unsigned int volume)
{
    voice->note_volume = volume;
    genmidi_voice_t *opl_voice = &voice->current_instr->voices[voice->current_instr_voice];

    unsigned int midi_volume = 2 * (volume_mapping_table[voice->channel->volume] + 1);
    unsigned int full_volume = (volume_mapping_table[voice->note_volume] * midi_volume) >> 9;
    // Boost OPL2 output volume (~+12 dB) so background music is clearly audible.
    full_volume = (full_volume * 3) / 2 + 8;
    if (full_volume > 0x3f)
        full_volume = 0x3f;
    unsigned int car_volume = 0x3f - full_volume;

    if (car_volume != (voice->car_volume & 0x3f))
    {
        voice->car_volume = car_volume | (voice->car_volume & 0xc0);
        OPL_WriteRegister((OPL_REGS_LEVEL + voice->op2) | voice->array, voice->car_volume);

        if ((opl_voice->feedback & 0x01) != 0 && opl_voice->modulator.level != 0x3f)
        {
            unsigned int mod_volume = opl_voice->modulator.level;
            if (mod_volume < car_volume)
            {
                mod_volume = car_volume;
            }

            mod_volume |= voice->mod_volume & 0xc0;

            if (mod_volume != voice->mod_volume)
            {
                voice->mod_volume = mod_volume;
                OPL_WriteRegister((OPL_REGS_LEVEL + voice->op1) | voice->array,
                                  mod_volume | (opl_voice->modulator.scale & 0xc0));
            }
        }
    }
}

static void SetVoicePan(opl_voice_t *voice, unsigned int pan)
{
    voice->reg_pan = pan;
    genmidi_voice_t *opl_voice = &voice->current_instr->voices[voice->current_instr_voice];

    OPL_WriteRegister((OPL_REGS_FEEDBACK + voice->index) | voice->array,
                      opl_voice->feedback | pan);
}

// -----------------------------------------------------------------------------
// Channel Operations
// -----------------------------------------------------------------------------

static void SetChannelVolume(opl_channel_data_t *channel, unsigned int volume)
{
    channel->volume_base = volume;

    if ((int)volume > current_music_volume)
    {
        volume = current_music_volume;
    }

    channel->volume = volume;

    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel == channel)
        {
            SetVoiceVolume(voice_alloced_list[i], voice_alloced_list[i]->note_volume);
        }
    }
}

static void SetChannelPan(opl_channel_data_t *channel, unsigned int pan)
{
    unsigned int reg_pan = 0x30; // Center (both channels on OPL3, default for OPL2)

    if (is_opl3)
    {
        if (pan >= 96)
            reg_pan = 0x10; // Left
        else if (pan <= 48)
            reg_pan = 0x20; // Right
        else
            reg_pan = 0x30; // Center
    }

    if (channel->pan != (int)reg_pan)
    {
        channel->pan = reg_pan;
        for (int i = 0; i < voice_alloced_num; i++)
        {
            if (voice_alloced_list[i]->channel == channel)
            {
                SetVoicePan(voice_alloced_list[i], reg_pan);
            }
        }
    }
}

static void UpdateChannelFrequencies(int ch)
{
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel == &channels[ch])
        {
            UpdateVoiceFrequency(voice_alloced_list[i]);
        }
    }
}

static void AllNotesOff(opl_channel_data_t *channel)
{
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (channel == NULL || voice_alloced_list[i]->channel == channel)
        {
            ReleaseVoice(i);
            i--;
        }
    }
}

// -----------------------------------------------------------------------------
// Note Triggering & Release
// -----------------------------------------------------------------------------

static void VoiceKeyOn(opl_channel_data_t *channel,
                       genmidi_instr_t *instrument,
                       unsigned int instrument_voice,
                       unsigned int note,
                       unsigned int key,
                       unsigned int volume)
{
    if (voice_free_num == 0)
    {
        ReplaceExistingVoice();
    }
    if (voice_free_num == 0)
    {
        return;
    }

    opl_voice_t *voice = GetFreeVoice();
    if (!voice)
        return;

    voice->channel = channel;
    voice->key = key;

    if ((instrument->flags & GENMIDI_FLAG_FIXED) != 0)
    {
        voice->note = instrument->fixed_note;
    }
    else
    {
        voice->note = note;
    }

    voice->reg_pan = channel->pan;

    SetVoiceInstrument(voice, instrument, instrument_voice);
    SetVoiceVolume(voice, volume);

    voice->freq = 0;
    UpdateVoiceFrequency(voice);
}

static void KeyOn(int ch, int key, int volume)
{
    genmidi_instr_t *instr;
    int note = key;

    if (ch == 15) // MUS percussion channel
    {
        if (key < 35 || key > 81)
            return;
        instr = &percussion_instrs[key - 35];
        note = 60;
    }
    else
    {
        instr = channels[ch].instrument;
    }

    bool double_voice = (instr->flags & GENMIDI_FLAG_2VOICE) != 0;

    VoiceKeyOn(&channels[ch], instr, 0, note, key, volume);
    if (double_voice && is_opl3)
    {
        VoiceKeyOn(&channels[ch], instr, 1, note, key, volume);
    }
}

static void KeyOff(int ch, int key)
{
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel == &channels[ch] &&
            voice_alloced_list[i]->key == (unsigned int)key)
        {
            ReleaseVoice(i);
            i--;
        }
    }
}

// -----------------------------------------------------------------------------
// OPL Chip Initialization & Register Reset
// -----------------------------------------------------------------------------

static void OPL_InitRegisters(bool opl3)
{
    // Reset timers & enable interrupts
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);

    // Waveform select enable
    OPL_WriteRegister(OPL_REG_WAVEFORM_ENABLE, 0x20);

    if (opl3)
    {
        OPL_WriteRegister(OPL_REG_OPL3_NEW, 0x01); // Enable OPL3 extensions
    }

    // Set all operator volume levels to silent (0x3F)
    for (int r = OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + 0x15; ++r)
    {
        OPL_WriteRegister(r, 0x3f);
        if (opl3) OPL_WriteRegister(r | 0x100, 0x3f);
    }

    // Clear attack, sustain, waveform registers
    for (int r = OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + 0x15; ++r)
    {
        OPL_WriteRegister(r, 0x00);
        if (opl3) OPL_WriteRegister(r | 0x100, 0x00);
    }

    // Key-off all voices
    for (int r = OPL_REGS_FREQ_2; r <= OPL_REGS_FREQ_2 + 8; ++r)
    {
        OPL_WriteRegister(r, 0x00);
        if (opl3) OPL_WriteRegister(r | 0x100, 0x00);
    }

    // Set feedback / pan (default center 0x30)
    for (int r = OPL_REGS_FEEDBACK; r <= OPL_REGS_FEEDBACK + 8; ++r)
    {
        OPL_WriteRegister(r, 0x30);
        if (opl3) OPL_WriteRegister(r | 0x100, 0x30);
    }

    // Keyboard split mode off
    OPL_WriteRegister(OPL_REG_FM_MODE, 0x00);
}

static bool OPL_Detect(void)
{
    // Reset timer 1 and 2
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);

    uint8_t stat1 = OPL_PortRead(OPL_PORT_BASE);

    // Set timer 1 to 0xFF and start it
    OPL_WriteRegister(OPL_REG_TIMER1, 0xff);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x21);

    for (volatile int i = 0; i < 20000; i++)
    {
        OPL_PortRead(OPL_PORT_BASE);
    }

    uint8_t stat2 = OPL_PortRead(OPL_PORT_BASE);

    // Reset timers
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);

    if ((stat1 & 0xe0) == 0x00 && (stat2 & 0xc0) != 0)
    {
        uint8_t s = OPL_PortRead(OPL_PORT_BASE) & 0x06;
        if (s == 0)
        {
            is_opl3 = true;
        }
        else
        {
            is_opl3 = false;
        }
        return true;
    }

    // Fallback: Default to OPL3 (or OPL2) on x86 PC
    is_opl3 = true;
    return true;
}

static bool LoadInstrumentTable(void)
{
    int lumpnum = W_CheckNumForName("GENMIDI");
    if (lumpnum < 0)
    {
        return false;
    }

    uint8_t *lump = (uint8_t *)W_CacheLumpNum(lumpnum, PU_STATIC);
    if (!lump)
    {
        return false;
    }

    // Skip 8-byte header "#OPL_II#"
    main_instrs = (genmidi_instr_t *)(lump + 8);
    percussion_instrs = main_instrs + GENMIDI_NUM_INSTRS;

    return true;
}

// -----------------------------------------------------------------------------
// music_module_t Implementation
// -----------------------------------------------------------------------------

static bool I_OPL_Init(void)
{
    if (opl_initialized)
        return true;

    if (!LoadInstrumentTable())
        return false;

    OPL_Detect();
    num_opl_voices = is_opl3 ? OPL_NUM_VOICES_OPL3 : OPL_NUM_VOICES_OPL2;

    OPL_InitRegisters(is_opl3);
    InitVoices();

    for (int i = 0; i < 16; i++)
    {
        channels[i].instrument = &main_instrs[0];
        channels[i].volume_base = 127;
        channels[i].volume = current_music_volume;
        channels[i].pan = 0x30;
        channels[i].bend = 0;
        channels[i].last_velocity = 100;
    }

    opl_initialized = true;
    return true;
}

static void I_OPL_Shutdown(void)
{
    if (!opl_initialized)
        return;

    AllNotesOff(NULL);
    OPL_InitRegisters(is_opl3);

    music_playing = false;
    music_paused = false;
    current_song = NULL;
    opl_initialized = false;
}

static void I_OPL_SetMusicVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 127) volume = 127;

    current_music_volume = volume;

    for (int i = 0; i < 16; i++)
    {
        SetChannelVolume(&channels[i], channels[i].volume_base);
    }
}

static void I_OPL_PauseMusic(void)
{
    if (!music_playing || music_paused)
        return;

    music_paused = true;
    AllNotesOff(NULL);
}

static void I_OPL_ResumeMusic(void)
{
    if (!music_playing || !music_paused)
        return;

    music_paused = false;
    last_poll_ms = I_GetTimeMS();
    accum_ticks = 0;
}

static void *I_OPL_RegisterSong(void *data, int len)
{
    if (!data || len < 16)
        return NULL;

    const uint8_t *d = (const uint8_t *)data;

    // Validate MUS magic: "MUS\x1A"
    if (d[0] != 'M' || d[1] != 'U' || d[2] != 'S' || d[3] != 0x1A)
    {
        return NULL;
    }

    uint16_t score_len = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
    uint16_t score_start = (uint16_t)d[6] | ((uint16_t)d[7] << 8);

    if ((int)score_start + (int)score_len > len)
    {
        score_len = (uint16_t)(len - score_start);
    }

    mus_song_t *song = (mus_song_t *)malloc(sizeof(mus_song_t));
    if (!song)
        return NULL;

    song->data = d;
    song->len = len;
    song->score_start = score_start;
    song->score_len = score_len;

    return (void *)song;
}

static void I_OPL_UnRegisterSong(void *handle)
{
    if (!handle)
        return;

    mus_song_t *song = (mus_song_t *)handle;
    if (current_song == song)
    {
        current_song = NULL;
        music_playing = false;
        AllNotesOff(NULL);
    }

    free(song);
}

static void I_OPL_PlaySong(void *handle, bool looping)
{
    if (!handle)
        return;

    mus_song_t *song = (mus_song_t *)handle;
    current_song = song;
    song_looping = looping;
    song_pos = song->data + song->score_start;
    song_end = song->data + song->score_start + song->score_len;
    song_delay = 0;
    accum_ticks = 0;
    last_poll_ms = I_GetTimeMS();

    for (int i = 0; i < 16; i++)
    {
        channels[i].instrument = &main_instrs[0];
        channels[i].volume_base = 127;
        channels[i].volume = current_music_volume;
        channels[i].pan = 0x30;
        channels[i].bend = 0;
        channels[i].last_velocity = 100;
    }

    AllNotesOff(NULL);
    music_playing = true;
    music_paused = false;
}

static void I_OPL_StopSong(void)
{
    if (!music_playing)
        return;

    music_playing = false;
    music_paused = false;
    current_song = NULL;
    AllNotesOff(NULL);
}

static bool I_OPL_MusicIsPlaying(void)
{
    return music_playing && !music_paused;
}

// -----------------------------------------------------------------------------
// MUS Event Player (140 Hz Poll Routine)
// -----------------------------------------------------------------------------

static void I_OPL_Poll(void)
{
    if (!music_playing || music_paused || !current_song || !song_pos)
        return;

    uint32_t now = I_GetTimeMS();
    uint32_t delta_ms = now - last_poll_ms;
    last_poll_ms = now;

    // Prevent bursts if frame rate dropped or during level transition
    if (delta_ms > 200) delta_ms = 200;

    accum_ticks += delta_ms * 140;
    int ticks_to_run = accum_ticks / 1000;
    accum_ticks %= 1000;

    while (ticks_to_run > 0 && music_playing)
    {
        if (song_delay > 0)
        {
            uint32_t step = ((uint32_t)ticks_to_run < song_delay) ? (uint32_t)ticks_to_run : song_delay;
            song_delay -= step;
            ticks_to_run -= step;
            if (song_delay > 0)
                break;
        }

        while (song_delay == 0 && music_playing)
        {
            if (song_pos >= song_end)
            {
                if (song_looping)
                {
                    song_pos = current_song->data + current_song->score_start;
                    AllNotesOff(NULL);
                }
                else
                {
                    music_playing = false;
                    AllNotesOff(NULL);
                    break;
                }
            }

            uint8_t desc = *song_pos++;
            int ch = desc & 0x0F;
            int ev = desc & 0x70;
            bool has_delay = (desc & 0x80) != 0;

            switch (ev)
            {
                case 0x00: // Release key
                {
                    uint8_t key = *song_pos++ & 0x7F;
                    KeyOff(ch, key);
                    break;
                }
                case 0x10: // Press key
                {
                    uint8_t key = *song_pos++;
                    uint8_t vol;
                    if (key & 0x80)
                    {
                        vol = *song_pos++ & 0x7F;
                        channels[ch].last_velocity = vol;
                    }
                    else
                    {
                        vol = channels[ch].last_velocity;
                    }
                    key &= 0x7F;
                    if (vol == 0)
                    {
                        KeyOff(ch, key);
                    }
                    else
                    {
                        KeyOn(ch, key, vol);
                    }
                    break;
                }
                case 0x20: // Pitch wheel
                {
                    uint8_t wheel = *song_pos++;
                    // MUS pitch bend: 0..255 (128 center), map to -64..+63
                    channels[ch].bend = (wheel / 2) - 64;
                    UpdateChannelFrequencies(ch);
                    break;
                }
                case 0x30: // System event
                {
                    uint8_t sys_cmd = *song_pos++;
                    if (sys_cmd == 10 || sys_cmd == 11) // All sounds/notes off
                    {
                        AllNotesOff(&channels[ch]);
                    }
                    else if (sys_cmd == 14) // Reset controllers
                    {
                        channels[ch].volume_base = 127;
                        channels[ch].volume = current_music_volume;
                        channels[ch].pan = 0x30;
                        channels[ch].bend = 0;
                        UpdateChannelFrequencies(ch);
                    }
                    break;
                }
                case 0x40: // Change controller
                {
                    uint8_t ctrl = *song_pos++;
                    uint8_t val = *song_pos++;
                    if (ctrl == 0) // Instrument change
                    {
                        if (ch != 15) // Not percussion
                        {
                            channels[ch].instrument = &main_instrs[val & 0x7F];
                        }
                    }
                    else if (ctrl == 3) // Volume
                    {
                        SetChannelVolume(&channels[ch], val & 0x7F);
                    }
                    else if (ctrl == 4) // Pan
                    {
                        SetChannelPan(&channels[ch], val & 0x7F);
                    }
                    break;
                }
                case 0x50: // End of measure
                    break;

                case 0x60: // Score end
                {
                    if (song_looping)
                    {
                        song_pos = current_song->data + current_song->score_start;
                        AllNotesOff(NULL);
                    }
                    else
                    {
                        music_playing = false;
                        AllNotesOff(NULL);
                    }
                    break;
                }
                case 0x70: // Unused (1 dummy data byte)
                    song_pos++;
                    break;
            }

            if (has_delay)
            {
                uint32_t delay = 0;
                for (;;)
                {
                    uint8_t b = *song_pos++;
                    delay = (delay * 128) + (b & 0x7F);
                    if ((b & 0x80) == 0)
                        break;
                }
                song_delay = delay;
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Sound Devices & Module Struct Export
// -----------------------------------------------------------------------------

static snddevice_t music_opl_devices[] =
{
    SNDDEVICE_ADLIB,
    SNDDEVICE_SB,
    SNDDEVICE_PAS,
    SNDDEVICE_GUS,
    SNDDEVICE_WAVEBLASTER,
    SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_GENMIDI,
    SNDDEVICE_AWE32,
    SNDDEVICE_CD,
};

music_module_t music_opl_module =
{
    music_opl_devices,
    sizeof(music_opl_devices) / sizeof(music_opl_devices[0]),
    I_OPL_Init,
    I_OPL_Shutdown,
    I_OPL_SetMusicVolume,
    I_OPL_PauseMusic,
    I_OPL_ResumeMusic,
    I_OPL_RegisterSong,
    I_OPL_UnRegisterSong,
    I_OPL_PlaySong,
    I_OPL_StopSong,
    I_OPL_MusicIsPlaying,
    I_OPL_Poll,
};
