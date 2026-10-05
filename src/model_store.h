#ifndef HOLLY_MODEL_STORE_H
#define HOLLY_MODEL_STORE_H
#include "vault.h"
/* Holly image pre-partition gap: marker 8; bank headers 9 and 1028. */
#define MODEL_STORE_MAX (1018u*512u)
struct model_store {
 struct holly_block_ops io;
 uint32_t size,sequence,bank,sector,crc;
 const uint8_t *pending;
 unsigned ready,busy;
 int error;
};
uint32_t model_crc(const void *data,unsigned size);
/* 1 restored, 0 empty, -1 disabled. Never writes outside LBA 8..2046. */
int model_store_open(struct model_store *,const struct holly_block_ops *,void *,unsigned);
int model_store_save(struct model_store *,const void *);
void model_store_tick(struct model_store *);
#endif
