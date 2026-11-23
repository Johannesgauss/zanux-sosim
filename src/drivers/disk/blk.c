#include "drivers/disk/blk.h"
#include "drivers/disk/ata.h"
#include "util/memory.h"

static uint8_t *s_ramdisk_base = NULL;
static size_t s_ramdisk_size = 0;

void blk_init_ramdisk(const void *base, size_t size_bytes)
{
	s_ramdisk_base = (uint8_t *)(uintptr_t)base;
	s_ramdisk_size = size_bytes;
}

bool blk_is_ramdisk(void)
{
	return s_ramdisk_base != NULL && s_ramdisk_size >= BLK_SECTOR_SIZE;
}

bool blk_read_sectors(uint32_t lba, uint8_t count, uint8_t *buf)
{
	if (!buf || count == 0)
		return false;

	if (s_ramdisk_base) {
		size_t offset = (size_t)lba * BLK_SECTOR_SIZE;
		size_t bytes = (size_t)count * BLK_SECTOR_SIZE;
		if (offset > s_ramdisk_size || bytes > s_ramdisk_size - offset)
			return false;
		memcpy(buf, s_ramdisk_base + offset, bytes);
		return true;
	}

	return ata_read_sectors(lba, count, buf);
}

bool blk_write_sectors(uint32_t lba, uint8_t count, const uint8_t *buf)
{
	if (!buf || count == 0)
		return false;

	if (s_ramdisk_base) {
		size_t offset = (size_t)lba * BLK_SECTOR_SIZE;
		size_t bytes = (size_t)count * BLK_SECTOR_SIZE;
		if (offset > s_ramdisk_size || bytes > s_ramdisk_size - offset)
			return false;
		memcpy(s_ramdisk_base + offset, buf, bytes);
		return true;
	}

	return ata_write_sectors(lba, count, buf);
}
