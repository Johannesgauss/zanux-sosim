/**
 * @file exec.h
 * @brief Process spawning, user memory, FAT32 file access, graphics, input,
 *        timing, and sound system calls.
 */

#ifndef SYS_EXEC_H
#define SYS_EXEC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Load a flat binary from the FAT32 root directory and spawn it in Ring 3.
 *
 * Converts @p name to 8.3 uppercase, reads the file via FAT32, spawns a new
 * process with its own address space, and blocks the caller (`PROC_BLOCK`)
 * until the child exits.
 *
 * @param name Filename in the root directory (e.g. `"DOOM.BIN"`).
 * @return New process ID on success, or -1 if the file cannot be found or loaded.
 */
int64_t sys_exec(const char *name);

/**
 * @brief Allocate user-space memory in the calling process's address space.
 *
 * @param size Number of bytes to allocate (rounded up to 8-byte alignment).
 * @return Virtual address of the allocated region cast to `int64_t`.
 */
int64_t sys_alloc(size_t size);

/**
 * @brief Query the size of, or read, a file from the FAT32 root directory.
 *
 * When @p dst is NULL or @p max_len is 0, performs a fast directory-only lookup
 * and returns the file's byte size without reading its data clusters.
 * Otherwise, copies up to @p max_len bytes into @p dst.
 *
 * @param name    Filename in the root directory (e.g. `"DOOM1.WAD"`).
 * @param dst     Destination buffer, or NULL to query file size only.
 * @param max_len Capacity of @p dst in bytes.
 * @return File size (when querying) or bytes copied on success, or -1 on failure.
 */
int64_t sys_read_file(const char *name, void *dst, size_t max_len);

/**
 * @brief Blit a 32-bit `0x00RRGGBB` pixel buffer to the Limine framebuffer.
 *
 * Clears the framebuffer when dimensions change, computes the largest integer
 * scale factor that fits the screen, centers the output, and writes scaled rows
 * via a cached RAM row buffer to avoid slow uncached VRAM reads.
 *
 * @param buf Source pixel array of `w * h` 32-bit pixels.
 * @param w   Source image width in pixels.
 * @param h   Source image height in pixels.
 * @return 0 on success, or -1 if @p buf is NULL, dimensions are 0, or no framebuffer exists.
 */
int64_t sys_fb_blit(const uint32_t *buf, uint32_t w, uint32_t h);

/**
 * @brief Pop the next raw PS/2 Set-1 scancode from the keyboard ring buffer.
 *
 * @return Scancode in range `0..255` (bit 7 set on key release), or -1 if empty.
 */
int64_t sys_poll_scancode(void);

/**
 * @brief Return elapsed monotonic milliseconds since timer startup.
 *
 * Derived from the 100 Hz PIT IRQ0 tick counter (`g_timer_ticks * 10`).
 *
 * @return Elapsed time in milliseconds.
 */
int64_t sys_ticks_ms(void);

/**
 * @brief Drive the PC speaker via PIT channel 2 and port `0x61`.
 *
 * @param divisor PIT channel 2 frequency divisor (`1193180 / freq_hz`),
 *                or `0` to mute the speaker.
 * @return Always returns 0.
 */
int64_t sys_pcspk(uint16_t divisor);

/**
 * @brief Perform whitelisted 8-bit port I/O for the AdLib/OPL2/OPL3 synth.
 *
 * Restricts access strictly to I/O ports `0x388..0x38B`.
 *
 * @param port     Target OPL I/O port (`0x388..0x38B`).
 * @param val      Byte value to write (ignored when @p is_write is 0).
 * @param is_write Non-zero to write @p val to @p port; 0 to read from @p port.
 * @return 0 on write, byte read (`0..255`) on read, or -1 if @p port is out of range.
 */
int64_t sys_opl_io(uint16_t port, uint8_t val, int is_write);

/**
 * @brief Enumerate valid entries in the FAT32 root directory by index.
 *
 * Skips deleted (`0xE5`), LFN (`0x0F`), and volume-label (`0x08`) entries and
 * formats the matching 8.3 name as a NUL-terminated string (e.g. `"HELLO.TXT"`).
 *
 * @param index    Zero-based index of the valid directory entry to retrieve.
 * @param name_out Destination buffer of at least 16 bytes for the formatted filename.
 * @param size_out Optional pointer to receive the entry's byte size (may be NULL).
 * @return 1 if an entry was found, 0 if @p index is past the end, or -1 on error.
 */
int64_t sys_readdir(size_t index, char name_out[16], uint32_t *size_out);

/**
 * @brief Start a real Ring 3 scheduling simulation (`SYS_SIM_START`, syscall 12).
 *
 * Spawns/reuses Ring 3 `WORKER.BIN` processes and arms the kernel IRQ0 scheduler
 * to run the requested policy in @p cfg. The caller should poll @ref sys_sim_poll()
 * until it returns `1`.
 *
 * @param cfg Pointer to a `struct sim_config` in caller address space.
 * @return `0` on success, `-1` on invalid config, unknown policy, or busy engine.
 */
int64_t sys_sim_start(const void *cfg);

/**
 * @brief Poll the active kernel scheduling simulation (`SYS_SIM_POLL`, syscall 13).
 *
 * @param out_trace Pointer to a `struct sim_trace` in caller address space.
 * @return `1` if finished and @p out_trace was populated, `0` if still running,
 *         or `-1` if no simulation is active or @p out_trace is NULL.
 */
int64_t sys_sim_poll(void *out_trace);

/**
 * @brief Queue unsigned 8-bit 11025 Hz mono PCM audio samples (`SYS_AUDIO_WRITE`, syscall 14).
 *
 * Feeds the Sound Blaster 16 DMA ring buffer (lazily initialized on first call).
 * Passing `pcm == NULL` silences the buffer.
 *
 * @param pcm Pointer to unsigned 8-bit PCM samples (`0x80` = silence), or NULL to silence.
 * @param len Number of sample bytes to write.
 * @return Number of bytes written (>= 0), or -1 if no SB16 hardware is present.
 */
int64_t sys_audio_write(const uint8_t *pcm, size_t len);

/**
 * @brief Retrieve the command-line arguments passed to the most recent `sys_exec()` (`SYS_GET_ARGS`, syscall 15).
 *
 * @param dst     Destination buffer for NUL-terminated argument string.
 * @param max_len Capacity of @p dst in bytes.
 * @return Length of copied string (excluding NUL), or -1 on invalid pointer/length.
 */
int64_t sys_get_args(char *dst, size_t max_len);

/**
 * @brief Submit a Ring 3 worker's live mini-window frame (`SYS_WIN_BLIT`, syscall 16).
 */
int64_t sys_win_blit(const uint32_t *buf, uint32_t w, uint32_t h);

/**
 * @brief Read a Ring 3 worker's latest mini-window frame (`SYS_WIN_READ`, syscall 17).
 */
int64_t sys_win_read(uint32_t slot, uint32_t *dst, uint32_t max_pixels);

#endif // SYS_EXEC_H
