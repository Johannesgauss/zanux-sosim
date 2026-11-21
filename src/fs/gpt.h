#ifndef FS_GPT_H
#define FS_GPT_H

#include "util/alias.h"
#include <stdbool.h>

struct gpt_partition_entry {
	uint64_t partition_type_guid[2]; // the type of the partition
	uint64_t unique_partition_guid[2]; // the guid of the partition
	uint64_t start_lba;
	uint64_t end_lba;
	uint64_t attr;
	uint8_t partition_name[72]; // why specifically 72 characters? It must have a historic reason, I see Linux git commits have the same limit.
} __attribute__((packed));

struct gpt_partition_entry *gpt_table_get(void);
struct gpt_partition_entry gpt_partition_get(uint64_t guid[2]);
bool gpt_parse_uuid(const char *uuid_str, uint64_t out[2]);
struct gpt_partition_entry gpt_partition_from_cmdline(const char *cmdline);


#endif //FS_GPT_H
