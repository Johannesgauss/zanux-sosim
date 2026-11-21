/**
 * @file sb16.h
 * @brief Sound Blaster 16 (SB16) 8-bit 11025 Hz PCM audio driver via ISA DMA channel 1.
 */

#ifndef DRIVERS_AUDIO_SB16_H
#define DRIVERS_AUDIO_SB16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Output sample rate configured on the SB16 DSP (matches Doom's native ds* SFX lumps). */
#define SB16_SAMPLE_RATE 11025U

/**
 * @brief Initialize the Sound Blaster 16 DSP and start auto-init 8-bit PCM DMA playback.
 *
 * Safe to call multiple times (idempotent after first successful init).
 *
 * @return true if an SB16 was detected at base port 0x220 and started; false otherwise.
 */
bool sb16_init(void);

/**
 * @brief Write unsigned 8-bit mono 11025 Hz PCM samples into the SB16 DMA ring buffer.
 *
 * Non-blocking: writes up to the currently free space ahead of the DMA playback cursor.
 *
 * @param pcm Pointer to unsigned 8-bit PCM samples (`0x80` = silence).
 * @param len Number of sample bytes to queue.
 * @return Number of bytes actually written, or -1 if SB16 is unavailable.
 */
int64_t sb16_write_pcm(const uint8_t *pcm, size_t len);

/**
 * @brief Silence the SB16 ring buffer (fills with `0x80`).
 */
void sb16_silence(void);

#endif // DRIVERS_AUDIO_SB16_H
