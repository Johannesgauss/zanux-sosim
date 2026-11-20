#include "drivers/disk/ata.h"
#include "util/panic.h"
#include "mem/kmalloc.h"
#include "util/memory.h"
#include "fs_macros.h"
#include "fat32.h"
#include "gpt.h"

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

#define FAT32_LFN_MARKER 0x0F
#define FAT32_EOC 0x0FFFFFF8
#define FAT32_ENTRIES_PER_SECTOR (SECTOR_SIZE / sizeof(uint32_t))
#define CLUSTER_BYTES (s_fat32_volume_id.bpb.sectors_per_cluster * SECTOR_SIZE)

static uint32_t s_fat_first_lba;
static uint32_t s_data_start_cluster_lba;
static size_t s_dir_entries_num;

static uint32_t s_fat_cache[FAT32_ENTRIES_PER_SECTOR];
static uint32_t s_fat_cache_lba = 0xFFFFFFFF;

static inline uint32_t cluster_to_lba(uint32_t cluster)
{
	return s_data_start_cluster_lba + (cluster - 2) * s_fat32_volume_id.bpb.sectors_per_cluster;
}

static inline uint32_t fat32_next_cluster(uint32_t cluster)
{
	uint32_t index = cluster % FAT32_ENTRIES_PER_SECTOR;
	uint32_t fat_lba = s_fat_first_lba + (cluster / FAT32_ENTRIES_PER_SECTOR);
	if (fat_lba != s_fat_cache_lba) {
		if (!ata_read_sectors(fat_lba, 1, (uint8_t *)s_fat_cache))
			panic("COULD NOT READ FAT SECTOR!");
		s_fat_cache_lba = fat_lba;
	}
	return s_fat_cache[index] & 0x0FFFFFFF;
}

static void *fat32_read_clusters(uint32_t start_cluster, size_t clusters_num)
{
	void *buf = kmalloc(clusters_num * CLUSTER_BYTES);
	uint8_t *dst = (uint8_t *)buf;
	uint32_t current_cluster = start_cluster;

	for (size_t c = 0; c < clusters_num && current_cluster >= 2 && current_cluster < FAT32_EOC; c++) {
		if (!ata_read_sectors(cluster_to_lba(current_cluster), s_fat32_volume_id.bpb.sectors_per_cluster, dst))
			panic("FILE COULD NOT BE READ");
		dst += CLUSTER_BYTES;
		current_cluster = fat32_next_cluster(current_cluster);
	}

	return buf;
}

void fat32_init(uint64_t guid[2])
{
	struct gpt_partition_entry fat_partition = gpt_partition_get(guid);

	if (!ata_read_sectors(fat_partition.start_lba, 1, (uint8_t *)&s_fat32_volume_id))
		panic("ATA FAILED TO READ FAT32 SECTOR.");

	if (s_fat32_volume_id.bootable_partition_signature[0] != 0x55 || s_fat32_volume_id.bootable_partition_signature[1] != 0xAA)
		panic("FAT32: CORRUPTED SIGNATURE");

	s_fat_first_lba = fat_partition.start_lba + s_fat32_volume_id.bpb.reserved_sectors;
	s_data_start_cluster_lba = s_fat_first_lba + s_fat32_volume_id.bpb.fat_num * s_fat32_volume_id.sectors_per_fat;
	s_dir_entries_num = CLUSTER_BYTES / sizeof(struct fat32_dir_entry);
}

struct fat32_dir_entry *fat32_open_dir(uint32_t cluster)
{
	return (struct fat32_dir_entry *)fat32_read_clusters(cluster, 1);
}

struct fat32_dir_entry *fat32_open_root_dir(void)
{
	return fat32_open_dir(s_fat32_volume_id.root_cluster);
}

void fat32_close_dir(struct fat32_dir_entry *dir)
{
	kfree(dir);
}

void fat32_close_file(struct file file)
{
	kfree(file.mptr);
}

struct file fat32_read_file(struct fat32_dir_entry *dir, char filename[FAT32_FILENAME_SIZE])
{
	struct fat32_dir_entry *file_entry = dir;
	for (size_t i = 0; i < s_dir_entries_num && file_entry->filename[0] != 0; i++, file_entry++) {
		if (file_entry->filename[0] == 0xE5 || (file_entry->attr & FAT32_LFN_MARKER) == FAT32_LFN_MARKER)
			continue;
		if (!memcmp(file_entry->filename, filename, FAT32_FILENAME_SIZE))
			goto FOUND;
	}
	panic("FILE NOT FOUND!");

FOUND:;
	uint32_t cluster = ((uint32_t)file_entry->cluster_high << 16) | file_entry->cluster_low;

	struct file file;
	file.start_cluster = cluster;
	file.size = file_entry->size;
	if (file.size == 0) {
		file.mptr = NULL;
		return file;
	}
	if (cluster < 2)
		panic("INVALID CLUSTER");

	size_t clusters_num = (file.size + CLUSTER_BYTES - 1) / CLUSTER_BYTES;
	file.mptr = fat32_read_clusters(cluster, clusters_num);

	return file;
}
