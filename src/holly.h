#ifndef HOLLY_BRAIN_H
#define HOLLY_BRAIN_H
#include <stdint.h>
#include "memory.h"
#include "lore.h"
#include "reference.h"
#define HOLLY_TOPIC_SIZE 96
#define HOLLY_REPLY_SIZE HOLLY_REFERENCE_REPLY
typedef void (*holly_emit_fn)(const char *text, void *context);
typedef int (*holly_web_command_fn)(const char *command,holly_emit_fn emit,void *context);
void holly_set_web_command(holly_web_command_fn);
/* Observe bounded completed reply text; never transport prompts or uploads. */
void holly_set_reply_observer(holly_emit_fn,void *);
void holly_set_diagnostics(holly_web_command_fn);
typedef int (*holly_history_append_fn)(const char *input,const char *output,void *context);
typedef int (*holly_history_get_fn)(unsigned back,char *input,unsigned input_capacity,
                                   char *output,unsigned output_capacity,void *context);
typedef unsigned (*holly_history_count_fn)(void *context);
typedef int (*holly_memory_add_fn)(const char *text,const char *source,
                                  unsigned confidence,uint32_t *id,void *context);
typedef unsigned (*holly_memory_count_fn)(void *context);
typedef int (*holly_memory_get_fn)(uint32_t id,struct holly_memory_item *item,
                                   void *context);
typedef int (*holly_memory_get_at_fn)(unsigned index,struct holly_memory_item *item,
                                     void *context);
typedef int (*holly_memory_update_fn)(uint32_t id,const char *text,
                                      const char *source,unsigned confidence,
                                      void *context);
typedef int (*holly_memory_forget_fn)(uint32_t id,void *context);
struct holly_memory_ops {
    holly_memory_add_fn add;
    holly_memory_count_fn count;
    holly_memory_get_fn get;
    holly_memory_get_at_fn get_at;
    holly_memory_update_fn update;
    holly_memory_forget_fn forget;
};
enum holly_expression { HOLLY_IDLE, HOLLY_LISTENING, HOLLY_SPEAKING, HOLLY_THINKING, HOLLY_BLINKING };
struct holly_session {
    unsigned persona; /* 0 Holly, 1 Hilly, 2 Queeg. Presentation never changes stored facts. */
    unsigned search_enabled,search_cached;uint32_t search_job;char search_topic[193];
    unsigned note_pending,document_focus,document_next;
    uint8_t document_hash[32];
    char document_keywords[96];
    char user_name[49];
    char episode_focus[81];
    unsigned name_loaded,series_focus;
    char topic[HOLLY_TOPIC_SIZE];
    char last_answer[HOLLY_REPLY_SIZE];
    uint32_t last_memory_id;
    unsigned learning_enabled;
    unsigned turns;
    unsigned dialogue_enabled;
    unsigned dialogue_model;
    unsigned personality_enabled;
    unsigned discussion_enabled;
    unsigned discussion_turns;
    unsigned discussion_profile;
    unsigned discussion_hypothetical;
    struct holly_lore lore;
    struct holly_reference reference;
    char dialogue_previous[640];
    enum holly_expression expression;
};
void holly_session_init(struct holly_session *session);
/* One trusted user utterance. SSH and voice can keep separate sessions. */
int holly_turn(struct holly_session *session,const char *input,
               holly_emit_fn emit,void *context);
void holly_set_history(holly_history_append_fn append,holly_history_get_fn get,
                       holly_history_count_fn count,void *context);
void holly_set_storage_card_sectors(uint32_t sectors);
void holly_set_memory(const struct holly_memory_ops *ops,void *context);
/* Transport-independent command processor; can later run over SSH. */
int holly_command(char *line,holly_emit_fn emit,void *context);
/* Guest conversation blocks administration/training. Shared assistant notes,
 * alarms and explicitly authorised automatic web-reference caching may persist. */
int holly_conversation_only(struct holly_session *,const char *,holly_emit_fn,void *);
void holly_set_telnet_command(holly_web_command_fn);
void holly_set_document_commands(holly_web_command_fn,holly_web_command_fn,holly_web_command_fn);
typedef int (*holly_document_chat_fn)(struct holly_session *,const char *,unsigned,holly_emit_fn,void *);
void holly_set_document_chat(holly_document_chat_fn);
void holly_assistant_tick(uint64_t);
uint32_t holly_clock_utc(void);
int holly_clock_seed(uint32_t,int,unsigned);
int holly_alarm_poll(char *,unsigned);
int holly_alarm_ack(unsigned);
int holly_browser_clock(const char *);
int holly_alarm_ack_text(const char *);
#endif
