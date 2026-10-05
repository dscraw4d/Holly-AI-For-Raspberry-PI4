#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vault.h"

#define DISK_SECTORS 8192u
static unsigned char disk[DISK_SECTORS*HOLLY_VAULT_SECTOR];
static uint32_t fail_write_lba=UINT32_MAX;
static int read_sector(uint32_t lba,uint8_t *out,void *context) {
    (void)context;if(lba>=DISK_SECTORS)return -1;memcpy(out,disk+lba*HOLLY_VAULT_SECTOR,512);return 0;
}
static int write_sector(uint32_t lba,const uint8_t *in,void *context) {
    (void)context;if(lba>=DISK_SECTORS||lba==fail_write_lba)return -1;
    memcpy(disk+lba*HOLLY_VAULT_SECTOR,in,512);return 0;
}
static void mbr_partition(uint32_t first,uint32_t sectors) {
    memset(disk,0,sizeof(disk));unsigned at=446;disk[at+4]=HOLLY_VAULT_PARTITION_TYPE;
    memcpy(disk+at+8,&first,4);memcpy(disk+at+12,&sectors,4);disk[510]=0x55;disk[511]=0xaa;
}
int main(void) {
    uint32_t first=8,sectors=HOLLY_VAULT_HISTORY_START_SECTOR+10u,found_first=0,found_sectors=0;
    mbr_partition(first,sectors);
    assert(holly_vault_find_partition(disk,&found_first,&found_sectors)==0);
    assert(found_first==first&&found_sectors==sectors);
    struct holly_block_ops io={read_sector,write_sector,0};static struct holly_vault vault;
    assert(holly_vault_mount(&vault,&io,first,sectors)==0);
    struct lesson saved[MIND_SLOTS],loaded[MIND_SLOTS];unsigned count=0;
    assert(holly_vault_load(&vault,loaded,&count)==0&&count==0);
    assert(vault.page_count==HOLLY_VAULT_JOURNAL_PAGES);
    assert(holly_vault_history_start(&vault)==0&&holly_vault_history_count(&vault)==0);
    memset(saved,0,sizeof(saved));strcpy(saved[0].question,"What is the ship called?");
    strcpy(saved[0].answer,"Holly AI Learning OS");
    assert(holly_vault_save(&vault,saved,1)==0);
    assert(holly_vault_load(&vault,loaded,&count)==0&&count==1);
    assert(!strcmp(loaded[0].question,saved[0].question)&&!strcmp(loaded[0].answer,saved[0].answer));
    strcpy(saved[0].answer,"A persistent ship computer");
    assert(holly_vault_save(&vault,saved,1)==0);
    /* A torn commit header must leave the previous complete page usable. */
    disk[(first+1u*HOLLY_VAULT_PAGE_SECTORS)*HOLLY_VAULT_SECTOR]=0;
    memset(loaded,0,sizeof(loaded));count=0;
    assert(holly_vault_load(&vault,loaded,&count)==0&&count==1);
    assert(!strcmp(loaded[0].answer,"Holly AI Learning OS"));
    for(unsigned i=0;i<7;i++) {
        char question[32],answer[32];
        snprintf(question,sizeof(question),"Question %u",i);
        snprintf(answer,sizeof(answer),"Answer %u",i);
        assert(holly_vault_history_append(&vault,question,answer)==0);
    }
    assert(holly_vault_history_count(&vault)==5);
    char question[HOLLY_VAULT_HISTORY_INPUT],answer[HOLLY_VAULT_HISTORY_OUTPUT];
    assert(holly_vault_history_get(&vault,0,question,sizeof(question),answer,sizeof(answer))==0);
    assert(!strcmp(question,"Question 6")&&!strcmp(answer,"Answer 6"));
    assert(holly_vault_history_get(&vault,4,question,sizeof(question),answer,sizeof(answer))==0);
    assert(!strcmp(question,"Question 2")&&!strcmp(answer,"Answer 2"));
    static struct holly_vault restarted,recovered,forgotten;
    assert(holly_vault_mount(&restarted,&io,first,sectors)==0);
    assert(holly_vault_load(&restarted,loaded,&count)==0&&count==1);
    assert(holly_vault_history_start(&restarted)==0&&holly_vault_history_count(&restarted)==5);
    assert(holly_vault_history_get(&restarted,0,question,sizeof(question),answer,sizeof(answer))==0);
    assert(!strcmp(question,"Question 6")&&!strcmp(answer,"Answer 6"));
    /* v2 migration reclaims old lesson pages without moving the chat ring. */
    fail_write_lba=first+HOLLY_VAULT_PAGE_SECTORS;
    assert(holly_vault_memory_start(&restarted,loaded,count)<0);
    fail_write_lba=UINT32_MAX;
    assert(holly_vault_load(&restarted,loaded,&count)==0&&count==1);
    assert(!strcmp(loaded[0].answer,"Holly AI Learning OS"));
    assert(holly_vault_memory_start(&restarted,loaded,count)==0);
    assert(restarted.format_version==2&&restarted.page_count==2);
    assert(holly_vault_memory_count(&restarted)==0);
    strcpy(loaded[0].answer,"Holly AI Learning OS v2");
    assert(holly_vault_save(&restarted,loaded,count)==0);
    uint32_t id=0;
    assert(holly_vault_memory_add(&restarted,
        "Holly keeps reviewed facts with their source and confidence.",
        "user",88,&id)==0&&id==1);
    struct holly_memory_item item;
    assert(holly_vault_memory_get(&restarted,id,&item)==0);
    assert(item.confidence==88&&!strcmp(item.source,"user"));
    assert(strstr(item.text,"reviewed facts"));
    assert(holly_vault_memory_update(&restarted,id,
        "Holly keeps reviewed facts, source, confidence, and revisions.",
        "captain correction",100)==0);
    assert(holly_vault_memory_get(&restarted,id,&item)==0);
    assert(item.confidence==100&&!strcmp(item.source,"captain correction"));
    /* A torn newest memory copy falls back to the last verified revision. */
    disk[(first+HOLLY_VAULT_MEMORY_START_SECTOR+1u)*HOLLY_VAULT_SECTOR]^=1u;
    assert(holly_vault_mount(&recovered,&io,first,sectors)==0);
    assert(holly_vault_load(&recovered,loaded,&count)==0&&count==1);
    assert(!strcmp(loaded[0].answer,"Holly AI Learning OS v2"));
    assert(holly_vault_history_start(&recovered)==0);
    assert(holly_vault_memory_start(&recovered,loaded,count)==0);
    assert(holly_vault_memory_count(&recovered)==1);
    assert(holly_vault_memory_get(&recovered,id,&item)==0);
    assert(item.confidence==88&&!strcmp(item.source,"user"));
    assert(holly_vault_history_count(&recovered)==5);
    assert(holly_vault_memory_update(&recovered,id,
        "Corrected fact survives reboot and can be retrieved.",
        "user correction",100)==0);
    assert(holly_vault_memory_forget(&recovered,id)==0);
    assert(holly_vault_memory_count(&recovered)==0);
    assert(holly_vault_memory_get(&recovered,id,&item)<0);
    assert(holly_vault_mount(&forgotten,&io,first,sectors)==0);
    assert(holly_vault_load(&forgotten,loaded,&count)==0&&count==1);
    assert(holly_vault_history_start(&forgotten)==0);
    assert(holly_vault_memory_start(&forgotten,loaded,count)==0);
    assert(holly_vault_memory_count(&forgotten)==0);
    assert(holly_vault_history_count(&forgotten)==5);
    assert(holly_vault_memory_add(&forgotten,"Replacement item.","user",100,&id)==0);
    assert(id==1&&holly_vault_memory_count(&forgotten)==1);
    puts("Holly Vault journal tests passed");
}
