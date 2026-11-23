#ifndef FS_FAT32_H
#define FS_FAT32_H

#include "util/alias.h"
#include <stdbool.h>

#define FAT32_FILENAME_SIZE 11
struct fat32_dir_entry {
	uint8_t filename[11];
	uint8_t attr;
	uint8_t unused[8]; // first byte is 0xE5. Why so much random conventions?
	uint16_t cluster_high;
	uint8_t unused2[4];
	uint16_t cluster_low;
	uint32_t size;
} __attribute__((packed));

typedef enum fat32_dir_entry_attr {
	FAT32_DIR_ENTRY_ATTR_READ_ONLY = 0,
	FAT32_DIR_ENTRY_ATTR_HIDDEN = 1,
	FAT32_DIR_ENTRY_ATTR_SYSTEM = 2,
	FAT32_DIR_ENTRY_ATTR_VOLUME_ID = 3,
	FAT32_DIR_ENTRY_ATTR_DIR = 4,
	FAT32_DIR_ENTRY_ATTR_ARCHIVE = 5,
} fat32_dir_entry_attr ;

struct file {
	virt_t mptr;
	uint32_t start_cluster;
	size_t size;
};

bool fat32_init(uint64_t start_lba);
struct fat32_dir_entry *fat32_open_root_dir(void);
struct fat32_dir_entry *fat32_open_dir(uint32_t cluster);
size_t fat32_dir_entries_count(void);
int64_t fat32_file_size(struct fat32_dir_entry *root_dir, char filename[FAT32_FILENAME_SIZE]);
void fat32_close_file(struct file file);
struct file fat32_read_file(struct fat32_dir_entry *root_dir, char filename[FAT32_FILENAME_SIZE]);
bool fat32_write_file(char filename[FAT32_FILENAME_SIZE], const void *data, size_t size);

#endif //FS_FAT32_H

