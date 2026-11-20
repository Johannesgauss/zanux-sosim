/**
 * @file ata.h
 * @brief Primary-bus ATA PIO disk driver (28-bit LBA, 512-byte sectors).
 *
 * Operates on the primary IDE/ATA channel (I/O ports 0x1F0..0x1F7), master
 * drive (0xE0). All transfers use polling PIO (BSY/DRQ/ERR) with a 100,000-
 * iteration timeout per wait stage.
 */

#ifndef DRIVERS_DISK_ATA_H
#define DRIVERS_DISK_ATA_H

#include <stdbool.h>
#include <stdint.h>

/** Size in bytes of a single ATA sector. */
#define ATA_SECTOR_SIZE 512

/**
 * @brief Read contiguous sectors from the primary ATA master drive (28-bit LBA PIO).
 *
 * Issues ATA READ SECTORS (0x20) and transfers 256 16-bit words per sector via
 * port 0x1F0. Only the lower 28 bits of @p lba are used (0..0x0FFFFFFF).
 *
 * @param lba   Starting 28-bit Logical Block Address.
 * @param count Number of 512-byte sectors to read. Must be > 0 (0 is rejected,
 *              not treated as 256).
 * @param buf   Destination buffer of at least `count * 512` bytes. Unaligned
 *              pointers are supported.
 *
 * @return true if all sectors were read without error or timeout; false if
 *         @p buf is NULL, @p count is 0, the drive sets ERR, or polling times out.
 */
bool ata_read_sectors(uint32_t lba, uint8_t count, uint8_t *buf);

/**
 * @brief Write contiguous sectors to the primary ATA master drive and flush cache.
 *
 * Issues ATA WRITE SECTORS (0x30), transfers 256 16-bit words per sector via
 * port 0x1F0, and then issues CACHE FLUSH (0xE7) waiting for BSY to clear.
 * Only the lower 28 bits of @p lba are used (0..0x0FFFFFFF).
 *
 * @param lba   Starting 28-bit Logical Block Address.
 * @param count Number of 512-byte sectors to write. Must be > 0 (0 is rejected,
 *              not treated as 256).
 * @param buf   Source buffer of at least `count * 512` bytes. Unaligned
 *              pointers are supported.
 *
 * @return true if all sectors were written and the cache flush completed; false
 *         if @p buf is NULL, @p count is 0, the drive sets ERR, or polling times out.
 */
bool ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t *buf);

#endif // DRIVERS_DISK_ATA_H
