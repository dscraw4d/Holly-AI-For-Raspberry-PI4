#include "mind.h"
struct lesson lessons[MIND_SLOTS];
unsigned lesson_count;
static mind_persist_fn persist;
static void *persist_context;
static int persist_error;

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
static int isword(char c) { c=lower(c); return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
static unsigned length(const char *s) { unsigned n=0; while(s[n]) ++n; return n; }
static void copy(char *to, const char *from, unsigned cap) {
    unsigned i=0; for (; i+1<cap && from[i]; ++i) to[i]=from[i]; to[i]=0;
}
static int same(const char *a, const char *b) {
    while (*a && *b) { if (lower(*a++) != lower(*b++)) return 0; }
    return *a == *b;
}
int mind_teach(const char *q, const char *a) {
    if (!length(q) || !length(a) || length(q)>=QUESTION_SIZE || length(a)>=ANSWER_SIZE) return -1;
    for (unsigned i=0;i<lesson_count;i++) if (same(lessons[i].question,q)) {
        struct lesson previous=lessons[i];
        copy(lessons[i].answer,a,ANSWER_SIZE);
        if (persist && persist(lessons,lesson_count,persist_context)) {
            lessons[i]=previous; persist_error=-1; return -3;
        }
        persist_error=0; return (int)i;
    }
    if (lesson_count == MIND_SLOTS) return -2;
    unsigned index=lesson_count++;
    copy(lessons[index].question,q,QUESTION_SIZE);
    copy(lessons[index].answer,a,ANSWER_SIZE);
    if (persist && persist(lessons,lesson_count,persist_context)) {
        for (unsigned i=0;i<sizeof(lessons[index].question);i++) lessons[index].question[i]=0;
        for (unsigned i=0;i<sizeof(lessons[index].answer);i++) lessons[index].answer[i]=0;
        lesson_count--; persist_error=-1; return -3;
    }
    persist_error=0; return (int)index;
}
static int word_in(const char *word, unsigned size, const char *haystack) {
    while (*haystack) {
        while (*haystack && !isword(*haystack)) ++haystack;
        const char *start=haystack;
        while (*haystack && isword(*haystack)) ++haystack;
        if ((unsigned)(haystack-start)==size) {
            unsigned i=0; while(i<size && lower(start[i])==lower(word[i])) ++i;
            if(i==size) return 1;
        }
    }
    return 0;
}
static int noise(const char *s,unsigned n) {
    static const char *stop[]={"what","who","where","when","why","how","the","is","are","can","a","an","do","does","tell","me","about"};
    for(unsigned k=0;k<sizeof(stop)/sizeof(stop[0]);k++) {
        if(length(stop[k])==n && word_in(s,n,stop[k])) return 1;
    }
    return 0;
}
int mind_ask(const char *q) {
    int best=-1, high=0;
    for (unsigned i=0;i<lesson_count;i++) {
        if(same(q,lessons[i].question)) return (int)i;
        int match=0,total=0;
        const char *p=q;
        while(*p) {
            while(*p && !isword(*p)) p++;
            const char *start=p;
            while(*p && isword(*p)) p++;
            unsigned n=(unsigned)(p-start);
            if(n>=2 && !noise(start,n)) {
                total++;
                if(word_in(start,n,lessons[i].question)) match++;
            }
        }
        int score=total ? (match*100/total) : 0;
        /* One shared word in a longer question is weak evidence. */
        if(score>high && score>=50 && (total<=1 || match>=2)) {
            high=score; best=(int)i;
        }
    }
    return best;
}

int mind_restore(const struct lesson *saved, unsigned count) {
    if ((!saved && count) || count>MIND_SLOTS) return -1;
    for (unsigned i=0;i<MIND_SLOTS;i++) {
        for (unsigned j=0;j<QUESTION_SIZE;j++) lessons[i].question[j]=0;
        for (unsigned j=0;j<ANSWER_SIZE;j++) lessons[i].answer[j]=0;
    }
    for (unsigned i=0;i<count;i++) lessons[i]=saved[i];
    lesson_count=count; persist_error=0; return 0;
}

void mind_set_persistence(mind_persist_fn save, void *context) {
    persist=save; persist_context=context; persist_error=0;
}

int mind_persistence_enabled(void) { return persist!=0; }
int mind_persistence_last_error(void) { return persist_error; }
