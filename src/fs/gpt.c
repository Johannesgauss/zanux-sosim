#include "drivers/disk/ata.h"
#include "util/panic.h"
#include "mem/kmalloc.h"
#include "util/memory.h"
#include "fs_macros.h"
#include "gpt.h"

static struct gpt_protective_mbr {
	uint8_t boot_indicator;	
	uint8_t chs_start[3];
	uint8_t os_type;
	uint8_t chs_end[3];
	struct {
		uint32_t start_lba;
		uint32_t end_lba;
	} lba;
} __attribute__((packed)) s_gpt_protective_mbr;

static struct gpt_partition_table_header {
	uint64_t signature;
	uint32_t gpt_revision;
	uint32_t header_size;
	uint32_t gpt_header_checksum; // "CRC32"
	uint32_t reserved;
	uint64_t my_lba; // UEFI docs call it this way, I was out of names, don't blame me (at least directly).
	uint64_t alternate_lba; // the backup one, at bottom
	uint64_t first_usable_lba;
	uint64_t last_usable_lba; 
	uint64_t guid[2];
	uint64_t partition_entry_lba;
	uint32_t partition_entries_num;
	uint32_t partition_entry_size;
	uint32_t partition_entry_checksum; // also "CRC32"
} __attribute__((packed)) *s_gpt_partition_table_header;

struct gpt_partition_entry *gpt_table_get()
{
	uint8_t gpt_protective_mbr_buf[SECTOR_SIZE];
	if(!ata_read_sectors(0, 1, gpt_protective_mbr_buf))
	// TODO: bad practice
		panic("ATA DRIVER COULD NOT READ IT!");
	s_gpt_protective_mbr = *(struct gpt_protective_mbr*)(gpt_protective_mbr_buf + 446);
	// note: I'm using only GPT for now
	if (s_gpt_protective_mbr.os_type != 0xEE)
		panic("Only GPT, not MBR");
	s_gpt_partition_table_header = kmalloc(SECTOR_SIZE);

	if(!ata_read_sectors(1, 1, (uint8_t*)s_gpt_partition_table_header))
		panic("ATA DRIVER COULD NOT READ IT!");
	uint32_t table_size = s_gpt_partition_table_header->partition_entry_size * s_gpt_partition_table_header->partition_entries_num;
	uint32_t table_sectors = (table_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
	struct gpt_partition_entry *gpt_pe_array = kmalloc(table_size);

	if(!ata_read_sectors(s_gpt_partition_table_header->partition_entry_lba, table_sectors, (uint8_t*)gpt_pe_array))
		panic("ATA DRIVER COULD NOT READ IT!");

	return gpt_pe_array;
}

struct gpt_partition_entry gpt_partition_get(uint64_t guid[2])
{
	struct gpt_partition_entry *gpt_table = gpt_table_get();
	size_t gpt_index = 0;
	while (gpt_index < s_gpt_partition_table_header->partition_entries_num) {
		if (gpt_table[gpt_index].unique_partition_guid[0] == guid[0] && gpt_table[gpt_index].unique_partition_guid[1] == guid[1])
			goto FOUND;
		gpt_index++;
	} 
	panic("GPT PARTITION NOT FOUND.");
FOUND:;
      return gpt_table[gpt_index];
}

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static bool parse_hex_byte(const char *s, uint8_t *out)
{
	int hi = hex_nibble(s[0]);
	int lo = hex_nibble(s[1]);
	if (hi < 0 || lo < 0)
		return false;
	*out = (uint8_t)((hi << 4) | lo);
	return true;
}

bool gpt_parse_uuid(const char *uuid_str, uint64_t out[2])
{
	if (!uuid_str)
		return false;

	static const uint8_t char_offsets[16] = {
		6, 4, 2, 0,
		11, 9,
		16, 14,
		19, 21,
		24, 26, 28, 30, 32, 34
	};

	if (uuid_str[8] != '-' || uuid_str[13] != '-' || uuid_str[18] != '-' || uuid_str[23] != '-')
		return false;

	uint8_t bytes[16];
	for (size_t i = 0; i < 16; i++) {
		if (!parse_hex_byte(uuid_str + char_offsets[i], &bytes[i]))
			return false;
	}

	memcpy(out, bytes, 16);
	return true;
}

struct gpt_partition_entry gpt_partition_from_cmdline(const char *cmdline)
{
	if (!cmdline)
		panic("MISSING KERNEL CMDLINE");

	static const char key[] = "root=PARTUUID=";
	const size_t key_len = sizeof(key) - 1;

	const char *p = cmdline;
	while (*p) {
		if (!memcmp(p, key, key_len)) {
			uint64_t guid[2];
			if (!gpt_parse_uuid(p + key_len, guid))
				panic("INVALID root=PARTUUID IN CMDLINE");
			return gpt_partition_get(guid);
		}
		p++;
	}

	panic("root=PARTUUID NOT FOUND IN CMDLINE");
}
