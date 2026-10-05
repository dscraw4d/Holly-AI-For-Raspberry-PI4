#ifndef HOLLY_VAULT_H
#define HOLLY_VAULT_H
#include <stdint.h>
#include "mind.h"
#include "memory.h"

/* Holly's data partition is deliberately not a general-purpose filesystem.
 * It is a bounded, append-by-page journal so a power loss cannot require a
 * directory, allocator, or recovery daemon to bring the ship computer back. */
#define HOLLY_VAULT_SECTOR 512u
#define HOLLY_DOCUMENT_SECTORS (16u*1025u)
#define HOLLY_VAULT_PARTITION_TYPE 0xDAu
#define HOLLY_VAULT_PAGE_SECTORS (1u+MIND_SLOTS)
#define HOLLY_VAULT_JOURNAL_PAGES 64u
#define HOLLY_VAULT_HISTORY_INPUT 320u
#define HOLLY_VAULT_HISTORY_OUTPUT 640u
#define HOLLY_VAULT_HISTORY_RECORD_SECTORS 2u
#define HOLLY_VAULT_HISTORY_START_SECTOR \
    (HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+2u)
#define HOLLY_VAULT_MEMORY_START_SECTOR (2u*HOLLY_VAULT_PAGE_SECTORS)
#define HOLLY_VAULT_MEMORY_END_SECTOR \
    (HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS)
#define HOLLY_VAULT_MEMORY_RECORD_COPIES 2u
#define HOLLY_VAULT_MEMORY_SLOT_COUNT \
    ((HOLLY_VAULT_MEMORY_END_SECTOR-HOLLY_VAULT_MEMORY_START_SECTOR)/ \
     HOLLY_VAULT_MEMORY_RECORD_COPIES)

typedef int (*holly_block_read_fn)(uint32_t lba, uint8_t *sector, void *context);
typedef int (*holly_block_write_fn)(uint32_t lba, const uint8_t *sector, void *context);
struct holly_block_ops {
    holly_block_read_fn read;
    holly_block_write_fn write;
    void *context;
};

struct holly_vault {
    struct holly_block_ops io;
    uint32_t first_lba;
    uint32_t sectors;
    uint32_t page_count;
    uint32_t current_page;
    uint32_t current_sequence;
    uint32_t history_capacity;
    uint32_t history_next;
    uint32_t history_count;
    uint32_t history_total;
    uint32_t history_sequence;
    unsigned history_meta_slot;
    unsigned ready;
    unsigned history_ready;
    unsigned format_version;
    unsigned memory_ready;
    uint32_t memory_count;
    uint32_t memory_revision[HOLLY_VAULT_MEMORY_SLOT_COUNT];
    uint8_t memory_copy[HOLLY_VAULT_MEMORY_SLOT_COUNT];
    uint8_t memory_active[HOLLY_VAULT_MEMORY_SLOT_COUNT];
    struct holly_memory_item memories[HOLLY_VAULT_MEMORY_SLOT_COUNT];
};

/* Locate the custom data partition in an MBR sector. */
int holly_vault_find_partition(const uint8_t *mbr, uint32_t *first_lba,
                               uint32_t *sectors);
/* Expand only Holly's marked image partition to the detected card capacity. */
int holly_vault_expand_image_partition(uint8_t *mbr,uint32_t card_sectors,
                                       uint32_t *first_lba,uint32_t *sectors);
int holly_vault_mount(struct holly_vault *vault, const struct holly_block_ops *io,
                      uint32_t first_lba, uint32_t sectors);
/* Returns zero for an empty or valid vault, negative for malformed I/O. */
int holly_vault_load(struct holly_vault *vault, struct lesson *out, unsigned *count);
/* Writes a complete snapshot; the header is committed only after all data. */
int holly_vault_save(struct holly_vault *vault, const struct lesson *saved,
                     unsigned count);
/* Migrate the old lesson journal safely, then index reviewable memories. */
int holly_vault_memory_start(struct holly_vault *vault,const struct lesson *saved,
                             unsigned count);
unsigned holly_vault_memory_count(const struct holly_vault *vault);
int holly_vault_memory_add(struct holly_vault *vault,const char *text,
                           const char *source,unsigned confidence,
                           uint32_t *id_out);
int holly_vault_memory_get(struct holly_vault *vault,uint32_t id,
                           struct holly_memory_item *item);
int holly_vault_memory_get_at(struct holly_vault *vault,unsigned index,
                              struct holly_memory_item *item);
int holly_vault_memory_update(struct holly_vault *vault,uint32_t id,
                              const char *text,const char *source,
                              unsigned confidence);
int holly_vault_memory_forget(struct holly_vault *vault,uint32_t id);
/* Conversation journal: newest records are addressed with `back == 0`. */
int holly_vault_history_start(struct holly_vault *vault);
int holly_vault_history_append(struct holly_vault *vault,const char *input,
                               const char *output);
int holly_vault_history_get(struct holly_vault *vault,unsigned back,
                            char *input,unsigned input_capacity,
                            char *output,unsigned output_capacity);
unsigned holly_vault_history_count(const struct holly_vault *vault);
int holly_vault_reserve_documents(struct holly_vault *,uint32_t *first_lba);
const char *holly_vault_status(const struct holly_vault *vault);

uint32_t holly_vault_document_history_capacity(uint32_t sectors);
int holly_vault_reserve_documents_large(struct holly_vault *,uint32_t *,uint32_t *);
#endif
