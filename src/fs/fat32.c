#include "drivers/disk/blk.h"
#include "util/panic.h"
#include "mem/kmalloc.h"
#include "util/memory.h"
#include "fs_macros.h"
#include "fat32.h"

struct fat_bpb {
	uint8_t jmp[3];
	uint8_t oem[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_num;
	uint16_t root_dir_entry_num;
	uint16_t total_sectors;
	uint8_t media_descriptor_type;
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t heads_num;
	uint32_t hidden_sectors_num;
	uint32_t large_sector_count;
} __attribute__((packed));

static struct fat32_volume_id {
	struct fat_bpb bpb;

	uint32_t sectors_per_fat;
	uint16_t flags;
	uint16_t fat_number_version; // high byte is the major version, low byte the minor one
	uint32_t root_cluster;
	uint16_t sector_num_fsinfo;
	uint16_t sector_num_backup_boot_sector;
	uint8_t zeros[12];
	uint8_t drive_num;
	uint8_t nt_flags;
	uint8_t signature;
	uint32_t volume_id_serial_num;
	uint8_t volume_label_string[11];
	uint64_t system_identifier_string; // ALWAYS "FAT32 "
	uint8_t boot_code[420];
	uint8_t bootable_partition_signature[2]; // 0xAA55
} __attribute__((packed)) s_fat32_volume_id;

struct fsinfo {
	uint32_t lead_signature;
	uint8_t reserved[480]; // Do NOT use them! (according to docs)
	uint32_t signature2; // Must be 0x61417272, just...why?
	uint32_t free_cluster_count; // 0xFFFFFFFF = no hint
	uint32_t next_cluster_count; // 0xFFFFFFFF = no hint
	uint8_t reserved2[12];
	uint32_t trail_signature; // 0xAA550000
} __attribute__((packed));

#define LBA_START_BIT 4
#define TYPE_CODE_START_BIT 11

static uint32_t s_fat_first_lba;
static uint32_t s_data_start_cluster_lba;
static uint32_t s_cached_sector_slice_fat_table[SECTOR_SIZE / 4];
size_t s_dir_entries_num;
static size_t s_cluster_bytes;

static struct fat32_dir_entry *s_root_dir;
#define CLUSTER_BYTES (s_fat32_volume_id.bpb.sectors_per_cluster * SECTOR_SIZE)

#define FAT32_LFN_MARKER 0x0F

#define FAT32_EOC 0x0FFFFFF8
static uint32_t s_cached_sector_slice_fat_lba = 0xFFFFFFFF;
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / sizeof(uint32_t))

#define FAT32_TABLE_ENTRY_SIZE 4 //bytes

bool fat32_init(uint64_t start_lba)
{
	if (!blk_read_sectors(start_lba, 1, (uint8_t*)&s_fat32_volume_id))
		return false;

	if (s_fat32_volume_id.bootable_partition_signature[0] != 0x55 || s_fat32_volume_id.bootable_partition_signature[1] != 0xAA)
		return false;

	s_fat_first_lba = start_lba + s_fat32_volume_id.bpb.reserved_sectors;
	s_data_start_cluster_lba = s_fat_first_lba + s_fat32_volume_id.bpb.fat_num * s_fat32_volume_id.sectors_per_fat;
	s_dir_entries_num = (s_fat32_volume_id.bpb.sectors_per_cluster * SECTOR_SIZE) / sizeof(struct fat32_dir_entry);
	return true;
}


static inline uint32_t cluster_to_lba(uint32_t cluster)
{
	return s_data_start_cluster_lba + (cluster - 2) * s_fat32_volume_id.bpb.sectors_per_cluster;
}

// read from cluster, TODO a better name for this function
static inline struct file fat32_open_cluster_as_file(uint32_t cluster, size_t clusters_num)
{
	struct file file;
	file.start_cluster = cluster;

	file.size = clusters_num * CLUSTER_BYTES;
	file.mptr = kmalloc(clusters_num * CLUSTER_BYTES);
	if (!blk_read_sectors(cluster_to_lba(file.start_cluster), s_fat32_volume_id.bpb.sectors_per_cluster, (uint8_t*)file.mptr))
		panic("COULD NOT READ FILE!");

	return file;
}

struct fat32_dir_entry *fat32_open_dir(uint32_t cluster)
{
	struct file dir_file = fat32_open_cluster_as_file(cluster, 1);
	return (struct fat32_dir_entry *) dir_file.mptr;					
}

struct fat32_dir_entry *fat32_open_root_dir(void)
{
	return fat32_open_dir(s_fat32_volume_id.root_cluster);
}

size_t fat32_dir_entries_count(void)
{
	return s_dir_entries_num;
}

void fat32_close_file(struct file file)
{
	kfree(file.mptr);
}

static inline uint32_t fat32_next_cluster(uint32_t cluster)
{
	uint32_t index = cluster % ENTRIES_PER_SECTOR;
	uint32_t new_fat_sector_slice_lba = s_fat_first_lba + (cluster/ ENTRIES_PER_SECTOR);
	if (new_fat_sector_slice_lba != s_cached_sector_slice_fat_lba) {
		if (!blk_read_sectors(new_fat_sector_slice_lba, 1, (uint8_t*)s_cached_sector_slice_fat_table))
			panic("COULD NOT READ FAT SECTOR!");
		s_cached_sector_slice_fat_lba = new_fat_sector_slice_lba;
	}
	return s_cached_sector_slice_fat_table[index] & 0x0FFFFFFF;
}


int64_t fat32_file_size(struct fat32_dir_entry *dir, char filename[FAT32_FILENAME_SIZE])
{
	struct fat32_dir_entry *file_entry = dir;
	for (size_t i = 0; i < s_dir_entries_num && file_entry->filename[0] != 0; i++, file_entry++) {
		if (file_entry->filename[0] == 0xE5 || (file_entry->attr & FAT32_LFN_MARKER) == FAT32_LFN_MARKER)
			continue;
		if (!memcmp(file_entry->filename, filename, FAT32_FILENAME_SIZE))
			return (int64_t)file_entry->size;
	}
	return -1;
}

struct file fat32_read_file(struct fat32_dir_entry *dir, char filename[FAT32_FILENAME_SIZE])
{
	struct fat32_dir_entry *file_entry = dir;
	for (size_t i = 0; i < s_dir_entries_num && file_entry->filename[0] != 0; i++, file_entry++)  {
		if (file_entry->filename[0] == 0xE5 || (file_entry->attr & FAT32_LFN_MARKER) == FAT32_LFN_MARKER)
			continue;
		if (!memcmp(file_entry->filename, filename, FAT32_FILENAME_SIZE))
			goto FOUND;
	}
	return (struct file){ .mptr = NULL, .start_cluster = 0, .size = 0 };
FOUND:;
	uint32_t cluster = (file_entry->cluster_high << 0x10) + file_entry->cluster_low;

	struct file file; 
	file.start_cluster = cluster;
	file.size = file_entry->size;
	if (file.size == 0) {
		file.mptr = NULL;
		goto RETURN;
	} else if (cluster < 2) {
		panic("INVALID CLUSTER");
		goto RETURN;
	}


	size_t clusters_num = (file.size + CLUSTER_BYTES - 1) / CLUSTER_BYTES;
	file.mptr = kmalloc(clusters_num * CLUSTER_BYTES);

	uint32_t current_cluster = file.start_cluster;

	uint8_t *dst = (uint8_t*) file.mptr;
	uint8_t spc = s_fat32_volume_id.bpb.sectors_per_cluster;
	while (current_cluster >= 2 && current_cluster < FAT32_EOC) {
		uint32_t run_start = current_cluster;
		uint32_t run_sectors = spc;
		uint32_t next = fat32_next_cluster(current_cluster);
		while (next == current_cluster + 1 && run_sectors + spc <= 128) {
			current_cluster = next;
			run_sectors += spc;
			next = fat32_next_cluster(current_cluster);
		}
		if (!blk_read_sectors(cluster_to_lba(run_start), (uint8_t)run_sectors, dst))
			panic("FILE COULD NOT BE READ");
		dst += (size_t)run_sectors * SECTOR_SIZE;
		current_cluster = next;
	}
		
RETURN:;
	return file;
}

static bool fat32_set_fat_entry(uint32_t cluster, uint32_t val)
{
	uint32_t fat_sec_off = cluster / ENTRIES_PER_SECTOR;
	uint32_t idx = cluster % ENTRIES_PER_SECTOR;
	uint32_t lba = s_fat_first_lba + fat_sec_off;

	uint32_t sec[ENTRIES_PER_SECTOR];
	if (!blk_read_sectors(lba, 1, (uint8_t *)sec))
		return false;
	sec[idx] = (sec[idx] & 0xF0000000U) | (val & 0x0FFFFFFFU);

	for (uint8_t f = 0; f < s_fat32_volume_id.bpb.fat_num; f++) {
		uint32_t flba = s_fat_first_lba + f * s_fat32_volume_id.sectors_per_fat + fat_sec_off;
		if (!blk_write_sectors(flba, 1, (const uint8_t *)sec))
			return false;
	}
	s_cached_sector_slice_fat_lba = 0xFFFFFFFFU;
	return true;
}

static uint32_t fat32_alloc_cluster(void)
{
	uint32_t sec[ENTRIES_PER_SECTOR];
	for (uint32_t s = 0; s < s_fat32_volume_id.sectors_per_fat; s++) {
		if (!blk_read_sectors(s_fat_first_lba + s, 1, (uint8_t *)sec))
			return 0;
		uint32_t start = (s == 0) ? 2U : 0U;
		for (uint32_t i = start; i < ENTRIES_PER_SECTOR; i++) {
			if ((sec[i] & 0x0FFFFFFFU) == 0U) {
				uint32_t cl = s * ENTRIES_PER_SECTOR + i;
				if (!fat32_set_fat_entry(cl, 0x0FFFFFFFU))
					return 0;
				return cl;
			}
		}
	}
	return 0;
}

static void fat32_free_chain(uint32_t start_cluster)
{
	uint32_t cur = start_cluster;
	while (cur >= 2U && cur < FAT32_EOC) {
		uint32_t next = fat32_next_cluster(cur);
		fat32_set_fat_entry(cur, 0U);
		cur = next;
	}
}

bool fat32_write_file(char filename[FAT32_FILENAME_SIZE], const void *data, size_t size)
{
	if (!filename || (size > 0 && !data))
		return false;

	struct fat32_dir_entry *root = fat32_open_root_dir();
	if (!root)
		return false;

	int found_idx = -1;
	int free_idx = -1;
	for (size_t i = 0; i < s_dir_entries_num; i++) {
		uint8_t b0 = root[i].filename[0];
		if (b0 == 0x00) {
			if (free_idx < 0)
				free_idx = (int)i;
			break;
		}
		if (b0 == 0xE5) {
			if (free_idx < 0)
				free_idx = (int)i;
			continue;
		}
		if ((root[i].attr & FAT32_LFN_MARKER) == FAT32_LFN_MARKER)
			continue;
		if (!memcmp(root[i].filename, filename, FAT32_FILENAME_SIZE)) {
			found_idx = (int)i;
			break;
		}
	}

	int slot = (found_idx >= 0) ? found_idx : free_idx;
	if (slot < 0) {
		kfree(root);
		return false;
	}

	if (found_idx >= 0) {
		uint32_t old_cl = ((uint32_t)root[slot].cluster_high << 16) | root[slot].cluster_low;
		if (old_cl >= 2U)
			fat32_free_chain(old_cl);
	}

	uint32_t first_cluster = 0;
	uint32_t prev_cluster = 0;
	size_t cbytes = CLUSTER_BYTES;
	uint8_t spc = s_fat32_volume_id.bpb.sectors_per_cluster;

	if (size > 0) {
		uint8_t *cbuf = (uint8_t *)kmalloc(cbytes);
		if (!cbuf) {
			kfree(root);
			return false;
		}
		const uint8_t *src = (const uint8_t *)data;
		size_t remaining = size;
		while (remaining > 0) {
			uint32_t cl = fat32_alloc_cluster();
			if (cl < 2U) {
				if (first_cluster >= 2U)
					fat32_free_chain(first_cluster);
				kfree(cbuf);
				kfree(root);
				return false;
			}
			if (first_cluster == 0)
				first_cluster = cl;
			if (prev_cluster >= 2U)
				fat32_set_fat_entry(prev_cluster, cl);
			prev_cluster = cl;

			size_t chunk = (remaining < cbytes) ? remaining : cbytes;
			memset(cbuf, 0, cbytes);
			memcpy(cbuf, src, chunk);
			if (!blk_write_sectors(cluster_to_lba(cl), spc, cbuf)) {
				fat32_free_chain(first_cluster);
				kfree(cbuf);
				kfree(root);
				return false;
			}
			src += chunk;
			remaining -= chunk;
		}
		kfree(cbuf);
	}

	memset(&root[slot], 0, sizeof(struct fat32_dir_entry));
	memcpy(root[slot].filename, filename, FAT32_FILENAME_SIZE);
	root[slot].attr = FAT32_DIR_ENTRY_ATTR_ARCHIVE;
	root[slot].cluster_high = (uint16_t)(first_cluster >> 16);
	root[slot].cluster_low  = (uint16_t)(first_cluster & 0xFFFFU);
	root[slot].size         = (uint32_t)size;

	bool ok = blk_write_sectors(cluster_to_lba(s_fat32_volume_id.root_cluster), spc, (const uint8_t *)root);
	kfree(root);
	return ok;
}
