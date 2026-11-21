/**
 * @file blk.h
 * @brief Block device abstraction supporting Limine RAM-disk modules and ATA PIO.
 *
 * When a RAM-disk image (e.g. `disk.img`) is provided by Limine as a boot module,
 * all sector reads are served directly from memory so Zanux can boot from USB
 * pendrives on bare metal without needing an xHCI/USB mass-storage driver.
 * Otherwise, reads fall back transparently to the legacy primary ATA PIO driver.
 */

#ifndef DRIVERS_DISK_BLK_H
#define DRIVERS_DISK_BLK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Size in bytes of a single logical block / sector. */
#define BLK_SECTOR_SIZE 512

/**
 * @brief Attach an in-memory disk image as the active block device.
 *
 * @param base       Virtual address of the raw disk image in RAM.
 * @param size_bytes Total size of the disk image in bytes.
 */
void blk_init_ramdisk(const void *base, size_t size_bytes);

/**
 * @brief Check whether reads are currently served from a RAM-disk module.
 *
 * @return true if a RAM-disk is active, false if using ATA PIO.
 */
bool blk_is_ramdisk(void);

/**
 * @brief Read contiguous 512-byte sectors from the active block device.
 *
 * @param lba   Starting Logical Block Address.
 * @param count Number of 512-byte sectors to read (must be > 0).
 * @param buf   Destination buffer of at least `count * 512` bytes.
 * @return true on success, false on invalid arguments, out-of-bounds, or I/O error.
 */
bool blk_read_sectors(uint32_t lba, uint8_t count, uint8_t *buf);

#endif // DRIVERS_DISK_BLK_H
