#include "drivers/disk/ata.h"
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
	if (!ata_read_sectors(start_lba, 1, (uint8_t*)&s_fat32_volume_id))
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
	if (!ata_read_sectors(cluster_to_lba(file.start_cluster), s_fat32_volume_id.bpb.sectors_per_cluster, (uint8_t*)file.mptr))
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
		if (!ata_read_sectors(new_fat_sector_slice_lba, 1, (uint8_t*)s_cached_sector_slice_fat_table))
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
		if (!ata_read_sectors(cluster_to_lba(run_start), (uint8_t)run_sectors, dst))
			panic("FILE COULD NOT BE READ");
		dst += (size_t)run_sectors * SECTOR_SIZE;
		current_cluster = next;
	}
		
RETURN:;
	return file;
}
