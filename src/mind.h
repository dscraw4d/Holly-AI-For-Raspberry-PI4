#ifndef VIPER_MIND_H
#define VIPER_MIND_H
#define MIND_SLOTS 32
#define QUESTION_SIZE 96
#define ANSWER_SIZE 192
struct lesson { char question[QUESTION_SIZE]; char answer[ANSWER_SIZE]; };
typedef int (*mind_persist_fn)(const struct lesson *lessons, unsigned count, void *context);
extern struct lesson lessons[MIND_SLOTS];
extern unsigned lesson_count;
int mind_teach(const char *question, const char *answer);
int mind_ask(const char *question);
/* Restore a validated snapshot without invoking the persistence hook. */
int mind_restore(const struct lesson *saved, unsigned count);
/* The kernel installs this after the Holly Vault has mounted. */
void mind_set_persistence(mind_persist_fn save, void *context);
int mind_persistence_enabled(void);
int mind_persistence_last_error(void);
#endif
