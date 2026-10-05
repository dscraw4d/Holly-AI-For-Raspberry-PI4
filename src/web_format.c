#include "web_format.h"
#include <string.h>
static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
static int equal(const char *a, unsigned n, const char *b) {
  unsigned i = 0;
  while (i < n && b[i]) {
    if (lower(a[i]) != lower(b[i]))
      return 0;
    i++;
  }
  return i == n && !b[i];
}
static int decimal(const char *p, unsigned n, unsigned *v) {
  unsigned x = 0;
  if (!n)
    return -1;
  for (unsigned i = 0; i < n; i++) {
    if (p[i] < '0' || p[i] > '9' || x > 32768u / 10u)
      return -1;
    x = x * 10u + (unsigned)(p[i] - '0');
  }
  if (x > 32768)
    return -1;
  *v = x;
  return 0;
}
int holly_http_feed(struct holly_http *h, const void *data, unsigned n) {
  if (!h || (!data && n) || n > HOLLY_HTTP_LIMIT - h->used)
    return -1;
  memcpy(h->raw + h->used, data, n);
  h->used += n;
  h->raw[h->used] = 0;
  unsigned end = 0;
  for (unsigned i = 0; i + 3 < h->used && i < 4096; i++)
    if (!memcmp(h->raw + i, "\r\n\r\n", 4)) {
      end = i + 4;
      break;
    }
  if (!end)
    return h->used >= 4096 ? -1 : 0;
  if (end < 16 || ((memcmp(h->raw, "HTTP/1.1 200 ", 13) &&
                   memcmp(h->raw, "HTTP/1.0 200 ", 13)) &&
      (!h->accept_ddg || (memcmp(h->raw,"HTTP/1.1 202 ",13) && memcmp(h->raw,"HTTP/1.0 202 ",13)))))
    return -1;
  unsigned at = 0;
  while (at + 1 < end && memcmp(h->raw + at, "\r\n", 2))
    at++;
  at += 2;
  unsigned length = 0, has_length = 0, chunked = 0, json = 0;
  while (at + 2 < end) {
    unsigned line = at;
    while (at + 1 < end && memcmp(h->raw + at, "\r\n", 2))
      at++;
    unsigned colon = line;
    while (colon < at && h->raw[colon] != ':')
      colon++;
    if (colon == at)
      return -1;
    unsigned value = colon + 1, tail = at;
    while (value < tail && h->raw[value] == ' ')
      value++;
    while (tail > value && h->raw[tail - 1] == ' ')
      tail--;
    if (equal(h->raw + line, colon - line, "content-length")) {
      if (has_length || decimal(h->raw + value, tail - value, &length))
        return -1;
      has_length = 1;
    } else if (equal(h->raw + line, colon - line, "transfer-encoding")) {
      if (chunked || !equal(h->raw + value, tail - value, "chunked"))
        return -1;
      chunked = 1;
    } else if (equal(h->raw + line, colon - line, "content-encoding")) {
      if (!equal(h->raw + value, tail - value, "identity"))
        return -1;
    } else if (equal(h->raw + line, colon - line, "content-type")) {
      unsigned semicolon = value;
      while (semicolon < tail && h->raw[semicolon] != ';')
        semicolon++;
      json = h->accept_xml ? (equal(h->raw+value,semicolon-value,"application/rss+xml")||equal(h->raw+value,semicolon-value,"application/xml")||equal(h->raw+value,semicolon-value,"text/xml")) : (equal(h->raw + value, semicolon - value, "application/json") || (h->accept_ddg && equal(h->raw+value,semicolon-value,"application/x-javascript")));
    }
    at += 2;
  }
  if (!json || (!has_length && !chunked) || (has_length && chunked))
    return -1;
  if (has_length) {
    if (h->used - end < length)
      return 0;
    if (h->used - end != length)
      return -1;
    memcpy(h->body, h->raw + end, length);
    h->body[length] = 0;
    h->body_size = length;
    return 1;
  }
  at = end;
  unsigned out = 0;
  while (at < h->used) {
    unsigned chunk = 0, digits = 0;
    while (at < h->used && h->raw[at] != '\r') {
      char c = lower(h->raw[at++]);
      unsigned d = c >= '0' && c <= '9'   ? (unsigned)(c - '0')
                   : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10)
                                          : 16;
      if (d > 15 || ++digits > 8 || chunk > 32768u / 16)
        return -1;
      chunk = chunk * 16 + d;
    }
    if (!digits)
      return -1;
    if (at + 1 >= h->used)
      return 0;
    if (memcmp(h->raw + at, "\r\n", 2))
      return -1;
    at += 2;
    if (chunk > 32768 - out)
      return -1;
    if (h->used - at < chunk + 2)
      return 0;
    if (memcmp(h->raw + at + chunk, "\r\n", 2))
      return -1;
    if (!chunk) {
      if (at + 2 != h->used)
        return -1;
      h->body[out] = 0;
      h->body_size = out;
      return 1;
    }
    memcpy(h->body + out, h->raw + at, chunk);
    out += chunk;
    at += chunk + 2;
  }
  return 0;
}
struct json {
  const char *p, *end;
};
static void spaces(struct json *j) {
  while (j->p < j->end &&
         (*j->p == ' ' || *j->p == '\n' || *j->p == '\r' || *j->p == '\t'))
    j->p++;
}
static int string(struct json *j, char *out, unsigned cap) {
  if (j->p == j->end || *j->p++ != '"')
    return -1;
  unsigned n = 0;
  while (j->p < j->end) {
    unsigned c = (unsigned char)*j->p++;
    if (c == '"') {
      if (cap)
        out[n] = 0;
      return 0;
    }
    if (c < 32)
      return -1;
    if (c == '\\') {
      if (j->p == j->end)
        return -1;
      c = (unsigned char)*j->p++;
      if (c == 'u') {
        if (j->end - j->p < 4)
          return -1;
        unsigned code = 0;
        for (unsigned i = 0; i < 4; i++) {
          char x = lower(*j->p++);
          unsigned d = x >= '0' && x <= '9'   ? (unsigned)(x - '0')
                       : x >= 'a' && x <= 'f' ? (unsigned)(x - 'a' + 10)
                                              : 16;
          if (d > 15)
            return -1;
          code = code * 16 + d;
        }
        c = code >= 32 && code < 127 ? code : '?';
      } else if (c == 'n' || c == 'r' || c == 't' || c == 'b' || c == 'f')
        c = ' ';
      else if (c != '"' && c != '\\' && c != '/')
        return -1;
    } else if (c >= 128) {
      if (c < 0xc2 || c > 0xf4)
        return -1;
      unsigned rest = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
      unsigned code = c & ((1u << (6 - rest)) - 1u);
      if ((unsigned)(j->end - j->p) < rest)
        return -1;
      for (unsigned i = 0; i < rest; i++) {
        unsigned next = (unsigned char)*j->p++;
        if ((next & 192) != 128)
          return -1;
        code = (code << 6) | (next & 63);
      }
      if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
          code < (rest == 1   ? 0x80u
                  : rest == 2 ? 0x800u
                              : 0x10000u))
        return -1;
      c = '?';
    }
    if (cap && n + 1 < cap)
      out[n++] = (char)c;
  }
  return -1;
}
static int skip(struct json *j, unsigned depth) {
  if (depth > 12)
    return -1;
  spaces(j);
  if (j->p == j->end)
    return -1;
  if (*j->p == '"')
    return string(j, 0, 0);
  if (*j->p == '{' || *j->p == '[') {
    char open = *j->p++, close = open == '{' ? '}' : ']';
    spaces(j);
    if (j->p < j->end && *j->p == close) {
      j->p++;
      return 0;
    }
    for (;;) {
      if (open == '{') {
        if (string(j, 0, 0))
          return -1;
        spaces(j);
        if (j->p == j->end || *j->p++ != ':')
          return -1;
      }
      if (skip(j, depth + 1))
        return -1;
      spaces(j);
      if (j->p == j->end)
        return -1;
      if (*j->p == close) {
        j->p++;
        return 0;
      }
      if (*j->p++ != ',')
        return -1;
      spaces(j);
    }
  }
  const char *begin = j->p;
  while (j->p < j->end && *j->p != ',' && *j->p != '}' && *j->p != ']' &&
         *j->p != ' ' && *j->p != '\n' && *j->p != '\r' && *j->p != '\t')
    j->p++;
  unsigned size = (unsigned)(j->p - begin);
  if ((size == 4 && !memcmp(begin, "true", 4)) ||
      (size == 4 && !memcmp(begin, "null", 4)) ||
      (size == 5 && !memcmp(begin, "false", 5)))
    return 0;
  const char *p = begin, *end = j->p;
  if (p < end && *p == '-')
    p++;
  if (p == end)
    return -1;
  if (*p == '0')
    p++;
  else if (*p >= '1' && *p <= '9') {
    do {
      p++;
    } while (p < end && *p >= '0' && *p <= '9');
  } else
    return -1;
  if (p < end && *p == '.') {
    p++;
    const char *start = p;
    while (p < end && *p >= '0' && *p <= '9')
      p++;
    if (p == start)
      return -1;
  }
  if (p < end && (*p == 'e' || *p == 'E')) {
    p++;
    if (p < end && (*p == '+' || *p == '-'))
      p++;
    const char *start = p;
    while (p < end && *p >= '0' && *p <= '9')
      p++;
    if (p == start)
      return -1;
  }
  return p == end ? 0 : -1;
}
int holly_wiki_excerpt(const char *body, unsigned size, char out[176]) {
  struct json j = {body, body + size};
  spaces(&j);
  if (j.p == j.end || *j.p++ != '{')
    return -1;
  char type[32] = {0}, text[1024] = {0};
  unsigned seen = 0, seen_type = 0;
  for (;;) {
    spaces(&j);
    if (j.p == j.end)
      return -1;
    if (*j.p == '}') {
      return -1;
    }
    char key[64];
    if (string(&j, key, sizeof(key)))
      return -1;
    spaces(&j);
    if (j.p == j.end || *j.p++ != ':')
      return -1;
    spaces(&j);
    if (!strcmp(key, "extract")) {
      if (seen++ || string(&j, text, sizeof(text)))
        return -1;
    } else if (!strcmp(key, "type")) {
      if (seen_type++ || string(&j, type, sizeof(type)))
        return -1;
    } else if (skip(&j, 0))
      return -1;
    spaces(&j);
    if (j.p == j.end)
      return -1;
    if (*j.p == '}') {
      j.p++;
      break;
    }
    if (*j.p++ != ',')
      return -1;
  }
  spaces(&j);
  if (j.p != j.end || seen != 1 || strcmp(type, "standard"))
    return -1;
  unsigned used = 0;
  for (unsigned i = 0; text[i] && used < 175; i++) {
    char c = text[i];
    if (c == '|')
      return -1;
    if (c == ' ' && (!used || out[used - 1] == ' '))
      continue;
    out[used++] = c;
    if ((c == '.' || c == '!') && (text[i + 1] == ' ' || !text[i + 1]))
      break;
  }
  if (used == 175) {
    while (used > 168 && out[used - 1] != ' ')
      used--;
    while (used && out[used - 1] == ' ')
      used--;
    out[used++] = '.';
    out[used++] = '.';
    out[used++] = '.';
  }
  out[used] = 0;
  return used ? 0 : -1;
}

/* DuckDuckGo Instant Answer: never promote RelatedTopics/disambiguation to an answer. */
int holly_search_parse(const char*body,unsigned size,char*out,unsigned cap,char*source,unsigned source_cap){
 if(!body||!out||!source||cap<1201||source_cap<513||size>32768)return -1;
 out[0]=source[0]=0;struct json j={body,body+size};spaces(&j);if(j.p==j.end||*j.p++!='{')return -1;
 char text[1201]={0},url[513]={0},type[8]={0};unsigned seen=0,entries=0;
 for(;;){spaces(&j);if(j.p==j.end)return -1;if(*j.p=='}'){if(entries)return -1;j.p++;break;}
  char key[64];if(string(&j,key,sizeof key))return -1;spaces(&j);if(j.p==j.end||*j.p++!=':')return -1;spaces(&j);unsigned bit=0;char*dest=0;unsigned n=0;
  if(!strcmp(key,"AbstractText")){bit=1;dest=text;n=sizeof text;}else if(!strcmp(key,"AbstractURL")){bit=2;dest=url;n=sizeof url;}else if(!strcmp(key,"Type")){bit=4;dest=type;n=sizeof type;}
  if(bit){if(seen&bit||string(&j,dest,n))return -1;seen|=bit;}else if(skip(&j,0))return -1;
  entries++;spaces(&j);if(j.p==j.end)return -1;if(*j.p=='}'){j.p++;break;}if(*j.p++!=',')return -1;
 }
 spaces(&j);if(j.p!=j.end)return -1;
 if((seen&7)!=7||strcmp(type,"A")||!text[0]||strlen(url)<9||memcmp(url,"https://",8))return 0;
 for(unsigned i=0;text[i];i++)if(text[i]=='<'||text[i]=='>'||(unsigned char)text[i]<32)return -1;
 for(unsigned i=0;url[i];i++)if((unsigned char)url[i]<=32||url[i]=='<'||url[i]=='>'||url[i]=='"')return -1;
 unsigned n=(unsigned)strlen(text);if(n==sizeof text-1){memcpy(text+n-3,"...",3);}memcpy(out,text,n+1);memcpy(source,url,strlen(url)+1);return 1;
}

/* Wikipedia Action API: one main-namespace article, never disambiguation. */
struct wiki_summary {char text[1201],url[513],title[193];unsigned seen,reject;};
static int wiki_object(struct json*j,unsigned level,struct wiki_summary*w){spaces(j);if(j->p==j->end||*j->p++!='{')return -1;unsigned seen=0;spaces(j);if(j->p<j->end&&*j->p=='}'){j->p++;return 0;}
 for(;;){char key[64];if(string(j,key,sizeof key))return -1;spaces(j);if(j->p==j->end||*j->p++!=':')return -1;spaces(j);unsigned bit=0;
 if(level==0&&!strcmp(key,"query")){bit=1;if(seen&bit||wiki_object(j,1,w))return -1;}
 else if(level==0&&!strcmp(key,"error")){w->reject=1;if(skip(j,0))return -1;}
 else if(level==1&&!strcmp(key,"pages")){bit=2;if(seen&bit||j->p==j->end||*j->p++!='[')return -1;spaces(j);if(j->p<j->end&&*j->p!=']'&&wiki_object(j,2,w))return -1;spaces(j);if(j->p==j->end||*j->p++!=']')return -1;}
 else if(level==2&&(!strcmp(key,"extract")||!strcmp(key,"fullurl")||!strcmp(key,"title"))){char*dest;unsigned cap;if(!strcmp(key,"extract")){bit=4;dest=w->text;cap=sizeof w->text;}else if(!strcmp(key,"fullurl")){bit=8;dest=w->url;cap=sizeof w->url;}else{bit=16;dest=w->title;cap=sizeof w->title;}if(seen&bit||string(j,dest,cap))return -1;w->seen|=bit;}
 else if(level==2&&!strcmp(key,"ns")){bit=32;if(seen&bit||j->p==j->end||*j->p++!='0'||(j->p<j->end&&*j->p>='0'&&*j->p<='9'))return -1;w->seen|=bit;}
 else if(level==2&&!strcmp(key,"pageprops")){bit=64;if(seen&bit||wiki_object(j,3,w))return -1;}
 else {if((level==3&&!strcmp(key,"disambiguation"))||(level==2&&!strcmp(key,"missing")))w->reject=1;if(skip(j,0))return -1;}
 seen|=bit;spaces(j);if(j->p==j->end)return -1;if(*j->p=='}'){j->p++;break;}if(*j->p++!=',')return -1;spaces(j);
 }return 0;}
int holly_search_wiki_parse(const char*body,unsigned size,char*out,unsigned cap,char*source,unsigned source_cap){if(!body||!out||!source||cap<1201||source_cap<513||size>32768)return -1;
 out[0]=source[0]=0;struct json j={body,body+size};struct wiki_summary w={0};if(wiki_object(&j,0,&w))return -1;spaces(&j);if(j.p!=j.end)return -1;if(w.reject||(w.seen&60)!=60||!w.text[0]||!w.title[0])return 0;
 const char*host="https://en.wikipedia.org/wiki/";if(memcmp(w.url,host,strlen(host))||!w.url[strlen(host)])return -1;
 for(unsigned i=0;w.text[i];i++){unsigned char c=(unsigned char)w.text[i];if(c=='<'||c=='>'||(c<32&&c!='\n'&&c!='\r'&&c!='\t'))return -1;if(c<32)w.text[i]=' ';}
 for(unsigned i=0;w.url[i];i++)if((unsigned char)w.url[i]<=32||w.url[i]=='<'||w.url[i]=='>'||w.url[i]=='"'||w.url[i]=='\\')return -1;
 const char*label="";unsigned n=(unsigned)strlen(label);memcpy(out,label,n);unsigned len=(unsigned)strlen(w.text);if(len>1200-n)len=1200-n;memcpy(out+n,w.text,len);out[n+len]=0;if(len==1200-n)memcpy(out+n+len-3,"...",3);memcpy(source,w.url,strlen(w.url)+1);return 1;}
