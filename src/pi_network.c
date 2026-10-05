#include "news.h"
#include "search.h"
#include "pi_network.h"
#ifdef HOLLY_SSH_PROVISIONED
#include "client.h"
#include "clock.h"
#include "dhcp.h"
#include "exception.h"
#include "genet_live.h"
#include "https.h"
#include "rng200.h"
#include "ssh_credentials_generated.h"
#include "tcp_stream.h"
#include "telnet.h"
#include "http_server.h"
#include <string.h>
static struct holly_rng200 random_device;
static struct holly_genet nic;
static struct holly_tcp_stream stream,guest_stream,http_stream;
static struct holly_telnet guest;
/* Boot defaults live in .data. Network recovery preserves manual off/on choices. */
static unsigned telnet_enabled=1,http_enabled=1;
static struct holly_web_server http_server;
static int telnet_equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int telnet_control(const char *line,holly_emit_fn emit,void *ctx){
 if(telnet_equal(line,"http")||telnet_equal(line,"http status")||telnet_equal(line,"http on")||telnet_equal(line,"http off")){
  if(telnet_equal(line,"http on"))http_enabled=1;
  if(telnet_equal(line,"http off")){http_enabled=0;holly_tcp_stream_link_lost(&http_stream);holly_web_clear(&http_server);}
  emit(http_enabled?"HTTP on: port 80, unencrypted LAN guest chat. Open http://PI_IP/ in a browser. Training stays on SSH. Starts on automatically after reboot.\n":"HTTP off for this boot. Use http on over SSH; starts on after reboot.\n",ctx);return 0;
 }
 if(telnet_equal(line,"telnet on"))telnet_enabled=1;
 else if(telnet_equal(line,"telnet off")){telnet_enabled=0;holly_tcp_stream_link_lost(&guest_stream);}
 else if(!telnet_equal(line,"telnet status")&&!telnet_equal(line,"telnet")){emit("Use telnet on/off/status.\n",ctx);return 0;}
 emit(telnet_enabled?"Telnet on: port 23, unencrypted LAN guest conversation. No training or personal memories. Starts on automatically after reboot.\n":"Telnet off for this boot. Use telnet on over SSH; starts on after reboot.\n",ctx);return 0;
}
static struct holly_dhcp dhcp;
static struct holly_client outbound;
static struct holly_https https;
static unsigned wiki_active,news_mode,search_mode;
static uint64_t news_last_start,search_last_start;
static const char*reader_host="en.wikipedia.org";
static uint64_t wiki_now, wiki_epoch, wiki_time_set, wiki_last_start;
static char wiki_status[512] = "Holly: Wikipedia reader idle. Set wiki time "
                               "UNIX_SECONDS, then wiki read Article Title.\n";
static char wiki_request[1280], wiki_url[97];
uint8_t holly_pi_network_ip[4];
int holly_pi_network_dhcp;
static uint8_t rx_buffers[256 * 2048] __attribute__((aligned(64)));
static uint8_t tx_buffers[256 * 2048] __attribute__((aligned(64)));
static uint8_t frame[1514];
static uint64_t last_tick, last_link, last_recovery;
unsigned holly_pi_network_restarts;
static uint64_t last_turn_time;
static unsigned displayed_turn;
static int random_bytes(uint8_t *p, size_t n, void *context) {
  (void)context;
  return holly_rng200_bytes(&random_device, p, n);
}
static int transmit(const uint8_t *p, unsigned n, void *context) {
  (void)context;
  return holly_genet_transmit(&nic, p, n);
}
static int web_write(uint8_t *p, unsigned n, void *context) {
  (void)context;
  return (int)holly_client_write(&outbound, p, n);
}
static int web_read(uint8_t *p, unsigned n, void *context) {
  (void)context;
  unsigned count = holly_client_read(&outbound, p, n);
  return count ? (int)count : outbound.eof ? -1 : 0;
}
static void web_message(const char *text) {
  unsigned i = 0;
  while (text[i] && i + 1 < sizeof(wiki_status)) {
    wiki_status[i] = text[i];
    i++;
  }
  wiki_status[i] = 0;
}
static void web_stop(const char *message) {
  holly_client_abort(&outbound);
  holly_https_destroy(&https);
  wiki_active = 0;
  if(news_mode&&holly_news_pending())holly_news_fail();
  if(search_mode&&holly_search_pending())holly_search_fail();
  search_mode=0;news_mode=0;reader_host="en.wikipedia.org";
  if (message)
    web_message(message);
}
static void append(char *out, const char *text) {
  unsigned n = 0;
  while (out[n])
    n++;
  while (*text)
    out[n++] = *text++;
  out[n] = 0;
}
static void web_capture(const char *text, void *context) {
  (void)context;
  unsigned used = 0;
  while (wiki_status[used])
    used++;
  while (*text && used + 1 < sizeof(wiki_status))
    wiki_status[used++] = *text++;
  wiki_status[used] = 0;
}
static int news_fetch(uint32_t epoch){
 if(wiki_active)return -2;
 if(!dhcp.leased)return -1;
 if(news_last_start&&wiki_now-news_last_start<30000000u)return -3;
 reader_host="feeds.bbci.co.uk";
 wiki_request[0]=0;append(wiki_request,"GET /news/world/rss.xml HTTP/1.1\r\nHost: feeds.bbci.co.uk\r\nUser-Agent: HollyOS/0.49.29 (https://blitter.ca)\r\nAccept: application/rss+xml, application/xml, text/xml\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
 if(holly_client_start(&outbound,stream.mac,dhcp.ip,dhcp.mask,dhcp.router,dhcp.dns,reader_host,443,transmit,random_bytes,0)){reader_host="en.wikipedia.org";return -1;}
 news_mode=1;wiki_active=1;wiki_epoch=epoch;wiki_time_set=wiki_now;news_last_start=wiki_now;return 0;
}
static int search_fetch(uint32_t epoch,const char*encoded,unsigned provider){
 if(wiki_active)return -2;
 if(!dhcp.leased)return -1;
 if(!provider&&search_last_start&&wiki_now-search_last_start<10000000u)return -3;
 if(strlen(encoded)>576)return -1;
 wiki_request[0]=0;
 if(provider){reader_host="en.wikipedia.org";append(wiki_request,"GET /w/api.php?action=query&format=json&formatversion=2&generator=search&gsrsearch=");append(wiki_request,encoded);append(wiki_request,"&gsrlimit=1&gsrnamespace=0&prop=extracts%7Cinfo%7Cpageprops&inprop=url&ppprop=disambiguation&exintro=1&explaintext=1&exchars=1200 HTTP/1.1\r\nHost: en.wikipedia.org\r\nUser-Agent: HollyOS/0.49.29 (https://blitter.ca)\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");}
 else {reader_host="api.duckduckgo.com";
 append(wiki_request,"GET /?q=");append(wiki_request,encoded);
 append(wiki_request,"&format=json&no_html=1&skip_disambig=1&no_redirect=1&t=hollyos HTTP/1.1\r\nHost: api.duckduckgo.com\r\nUser-Agent: HollyOS/0.49.29\r\nAccept: application/json, application/x-javascript\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");}
 if(holly_client_start(&outbound,stream.mac,dhcp.ip,dhcp.mask,dhcp.router,dhcp.dns,reader_host,443,transmit,random_bytes,0)){reader_host="en.wikipedia.org";return -1;}
 search_mode=provider?2:1;wiki_active=1;wiki_epoch=epoch;wiki_time_set=wiki_now;search_last_start=wiki_now;return 0;
}
static int web_command(const char *line, holly_emit_fn emit, void *context) {
  if (!strcmp(line, "wiki status")) {
    emit(wiki_status, context);
    return 0;
  }
  if (!strcmp(line, "wiki cancel")) {
    web_stop("Holly: Wikipedia request cancelled.\n");
    emit(wiki_status, context);
    return 0;
  }
  if (strlen(line) >= 10 && !memcmp(line, "wiki time ", 10)) {
    if (wiki_active) {
      emit("Holly: Finish or cancel the request before changing time.\n",
           context);
      return 0;
    }
    uint64_t epoch = 0;
    const char *p = line + 10;
    while (*p >= '0' && *p <= '9' && epoch <= 4102444800ull) {
      epoch = epoch * 10 + (unsigned)(*p - '0');
      p++;
    }
    if (*p || epoch < 1577836800ull || epoch > 4102444800ull) {
      emit("Holly: Use wiki time UNIX_SECONDS for the current UTC time.\n",
           context);
      return 0;
    }
    wiki_epoch = epoch;
    wiki_time_set = wiki_now;
    emit("Holly: Certificate validation time set for this boot.\n", context);
    return 0;
  }
  if (strlen(line) > 10 && !memcmp(line, "wiki read ", 10)) {
    if (wiki_active) {
      emit("Holly: A Wikipedia request is running. Use wiki status or wiki "
           "cancel.\n",
           context);
      return 0;
    }
    if (!wiki_epoch) {
      emit("Holly: Set wiki time UNIX_SECONDS first so I can check certificate "
           "dates.\n",
           context);
      return 0;
    }
    if (!dhcp.leased) {
      emit("Holly: I need a DHCP lease with DNS and a gateway to read "
           "Wikipedia.\n",
           context);
      return 0;
    }
    if (wiki_last_start && wiki_now - wiki_last_start < 5000000u) {
      emit(
          "Holly: Please wait five seconds before another Wikipedia request.\n",
          context);
      return 0;
    }
    const char *title = line + 10;
    unsigned n = (unsigned)strlen(title);
    if (!n || n > 48) {
      emit("Holly: Use an English article title up to 48 characters.\n",
           context);
      return 0;
    }
    char path[49];
    for (unsigned i = 0; i < n; i++) {
      char c = title[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-' ||
            c == '(' || c == ')')) {
        emit("Holly: Article titles currently support letters, numbers, "
             "spaces, _, -, and parentheses.\n",
             context);
        return 0;
      }
      path[i] = c == ' ' ? '_' : c;
    }
    path[n] = 0;
    wiki_url[0] = 0;
    append(wiki_url, "https://en.wikipedia.org/wiki/");
    append(wiki_url, path);
    wiki_request[0] = 0;
    append(wiki_request, "GET /api/rest_v1/page/summary/");
    append(wiki_request, path);
    append(wiki_request,
           " HTTP/1.1\r\nHost: en.wikipedia.org\r\nUser-Agent: "
           "HollyWikiReader/0.33 (https://blitter.ca; manual article "
           "request)\r\nAccept: application/json\r\nAccept-Encoding: "
           "identity\r\nConnection: close\r\n\r\n");
    if (holly_client_start(&outbound, stream.mac, dhcp.ip, dhcp.mask,
                           dhcp.router, dhcp.dns, "en.wikipedia.org", 443,
                           transmit, random_bytes, 0)) {
      web_stop("Holly: Couldn't start the outbound connection. No Wikipedia "
               "memory saved.\n");
      emit(wiki_status, context);
      return 0;
    }
    wiki_active = 1;
    wiki_last_start = wiki_now;
    web_message(
        "Holly: Resolving Wikipedia and connecting. Use wiki status.\n");
    emit(wiki_status, context);
    return 0;
  }
  emit("Holly: wiki time UNIX_SECONDS | wiki read Article Title | wiki status "
       "| wiki cancel\n",
       context);
  return 0;
}
static void web_poll(void) {
  if (!wiki_active)
    return;
  if(news_mode&&!holly_news_pending()){web_stop(0);return;}
  if(search_mode&&!holly_search_pending()){web_stop(0);return;}
  if (outbound.phase == HC_FAILED) {
    web_stop("Holly: DNS or outbound TCP failed/timed out. No Wikipedia memory "
             "saved.\n");
    return;
  }
  if (outbound.phase != HC_OPEN)
    return;
  if (wiki_active == 1) {
    uint64_t time = wiki_epoch + (wiki_now - wiki_time_set) / 1000000u;
    if (holly_https_start(&https, reader_host, wiki_request, time,
                          random_bytes, web_write, web_read, 0)) {
      web_stop("Holly: Couldn't initialize verified TLS. No Wikipedia memory "
               "saved.\n");
      return;
    }
    https.http.accept_xml=news_mode;https.http.accept_ddg=search_mode==1;
    wiki_active = 2;
    web_message("Holly: Checking Wikipedia's certificate and reading HTTPS. "
                "Use wiki status.\n");
  }
  int result = holly_https_poll(&https);
  if (result < 0) {
    web_stop("Holly: TLS certificate, connection, or HTTP validation failed. "
             "No Wikipedia memory saved.\n");
    return;
  }
  if (result == 1) {
    if(search_mode){int retry=holly_search_finish(https.http.body,https.http.body_size);search_mode=0;web_stop(0);if(retry==1)(void)holly_search_continue();return;}
    if(news_mode){(void)holly_news_finish(https.http.body,https.http.body_size);web_stop(0);return;}
    char fact[176], command[320];
    if (holly_wiki_excerpt(https.http.body, https.http.body_size, fact)) {
      web_stop("Holly: No usable standard article excerpt; disambiguation and "
               "invalid JSON are rejected.\n");
      return;
    }
    command[0] = 0;
    append(command, "wiki import ");
    append(command, fact);
    append(command, " | ");
    append(command, wiki_url);
    wiki_status[0] = 0;
    (void)holly_command(command, web_capture, 0);
    web_stop(0);
  }
}
static void sync_network_address(void) {
  if (!dhcp.configured && wiki_active)
    web_stop("Holly: DHCP configuration lost; Wikipedia request cancelled.\n");
  if (dhcp.configured) {
    unsigned changed = 0;
    for (unsigned j = 0; j < 4; j++)
      changed |= stream.ip[j] ^ dhcp.ip[j];
    if (changed) {
      holly_tcp_stream_link_lost(&stream);
      holly_tcp_stream_link_lost(&guest_stream);
      holly_tcp_stream_link_lost(&http_stream);holly_web_clear(&http_server);
      if (wiki_active)
        web_stop(
            "Holly: Network address changed; Wikipedia request cancelled.\n");
      for (unsigned j = 0; j < 4; j++)
        stream.ip[j] = guest_stream.ip[j] = http_stream.ip[j] = dhcp.ip[j];
    }
  } else if (stream.phase != HOLLY_STREAM_LISTEN || guest_stream.phase != HOLLY_STREAM_LISTEN || http_stream.phase != HOLLY_STREAM_LISTEN) {
    holly_tcp_stream_link_lost(&stream);
      holly_tcp_stream_link_lost(&guest_stream);
      holly_tcp_stream_link_lost(&http_stream);holly_web_clear(&http_server);
  }
  for (unsigned j = 0; j < 4; j++)
    holly_pi_network_ip[j] = dhcp.configured ? dhcp.ip[j] : 0;
  holly_pi_network_dhcp = dhcp.leased ? 2 : (dhcp.configured ? 1 : 0);
}
static void barrier(void *context) {
  (void)context;
  __asm__ volatile("dmb sy" ::: "memory");
}
static void delay(unsigned us, void *context) {
  (void)context;
  struct holly_clock c = holly_pi4_clock();
  uint64_t start, now;
  if (holly_clock_read(&c, &start))
    return;
  do {
    if (holly_clock_read(&c, &now))
      return;
  } while (now - start < us);
}
int holly_pi_network_status;
int holly_pi_network_start(void) {
  uint8_t digest[32], mac[6];
  nic.device = ether_pi4_device();
  uint32_t revision;
  if (holly_cpu_probe32(0xfd580000ul, &revision)) {
    holly_pi_network_status = -2;
    return -2;
  }
  unsigned version = (revision >> 24) & 15u;
  if (version != 6 && version != 7) {
    holly_pi_network_status = -2;
    return -2;
  }
  holly_pi_network_status = -1;
  random_device = holly_pi4_rng200();
  if (holly_rng200_start(&random_device))
    return -1;
  (void)holly_sha256_hash(holly_credentials.rsa_n, 256, digest);
  mac[0] = 2;
  for (unsigned i = 1; i < 6; i++)
    mac[i] = digest[i];
  nic.device = ether_pi4_device();
  nic.delay_us = delay;
  nic.barrier = barrier;
  nic.rx = rx_buffers;
  nic.tx = tx_buffers;
  nic.rx_physical = (uintptr_t)rx_buffers;
  nic.tx_physical = (uintptr_t)tx_buffers;
  if (holly_genet_start(&nic, mac)) {
    holly_pi_network_status = -2;
    return -2;
  }
  holly_tcp_stream_init(&stream, mac, holly_network_ip, &holly_credentials,
                        random_bytes, transmit, 0);
  holly_tcp_stream_init(&guest_stream,mac,holly_network_ip,0,random_bytes,transmit,0);
  guest_stream.local_port=23;guest_stream.app_start=holly_telnet_start;
  guest_stream.app_feed=holly_telnet_feed;guest_stream.app_close=holly_telnet_close;
  guest_stream.app_context=&guest;holly_set_telnet_command(telnet_control);
  holly_tcp_stream_init(&http_stream,mac,holly_network_ip,0,random_bytes,transmit,0);
  http_stream.local_port=80;http_stream.app_start=holly_web_start;http_stream.app_feed=holly_web_feed;
  http_stream.app_close=holly_web_close;http_stream.app_context=&http_server;
  http_server.random=random_bytes;http_server.random_context=0;
  holly_dhcp_init(&dhcp, mac, holly_network_ip, transmit, random_bytes, 0);
  holly_set_web_command(web_command);holly_news_bind(news_fetch);holly_search_bind(search_fetch);
  holly_pi_network_status = 1;
  return 0;
}
void holly_pi_network_poll(uint64_t us) {
  wiki_now = us;
  if (holly_pi_network_status < 0) {
    /* A stopped DMA ring must not leave all LAN services dead forever. */
    if(us>=last_recovery&&us-last_recovery<5000000u)return;
    last_recovery=us;
    if(wiki_active)web_stop("Holly: Network recovery cancelled the Wikipedia request.\n");
    holly_tcp_stream_link_lost(&stream);
    holly_tcp_stream_link_lost(&guest_stream);
    holly_tcp_stream_link_lost(&http_stream);holly_web_clear(&http_server);
    if(nic.active)holly_genet_stop(&nic);
    holly_pi_network_restarts++;
    last_tick=last_link=0;
    (void)holly_pi_network_start();
    return;
  }
  if (holly_pi_network_status < 1)return;
  if (!last_link || us - last_link >= 1000000u) {
    unsigned was_link = nic.link;
    int link = holly_genet_link(&nic);
    last_link = us;
    if (link < 0) {
      holly_genet_stop(&nic);
      holly_pi_network_status = -3;
      return;
    }
    if (was_link && !link) {
      holly_tcp_stream_link_lost(&stream);
      holly_tcp_stream_link_lost(&guest_stream);
      holly_tcp_stream_link_lost(&http_stream);holly_web_clear(&http_server);
      if (wiki_active)
        web_stop("Holly: Link lost; Wikipedia request cancelled.\n");
      displayed_turn = 0;
      last_turn_time = 0;
      last_tick = us;
    }
    if ((unsigned)link != was_link || (!last_tick && link)) {
      (void)holly_dhcp_link(&dhcp, link, us / 1000);
      for (unsigned j = 0; j < 4; j++)
        holly_pi_network_ip[j] = 0;
      holly_pi_network_dhcp = 0;
    }
    holly_pi_network_status = link ? 2 : 1;
  }
  if (!nic.link)
    return;
  (void)holly_dhcp_tick(&dhcp, us / 1000);
  sync_network_address();
  for (unsigned i = 0; i < 16; i++) {
    int n = holly_genet_receive(&nic, frame, sizeof(frame));
    if (n < 0) {
      holly_pi_network_status = -4;
      return;
    }
    if (!n)
      break;
    (void)holly_dhcp_receive(&dhcp, frame, (unsigned)n, us / 1000);
    sync_network_address();
    if (dhcp.configured)
      { (void)holly_tcp_stream_receive(&stream, frame, (unsigned)n);
        if(telnet_enabled)(void)holly_tcp_stream_receive(&guest_stream,frame,(unsigned)n);
        if(http_enabled)(void)holly_tcp_stream_receive(&http_stream,frame,(unsigned)n); }
    if (dhcp.configured && wiki_active)
      (void)holly_client_receive(&outbound, frame, (unsigned)n);
  }
  sync_network_address();
  if (!last_tick)
    last_tick = us;
  if (us - last_tick >= 1000) {
    uint64_t delta = (us - last_tick) / 1000;
    if (delta > 120000)
      delta = 120000;
    (void)holly_tcp_stream_tick(&stream, (uint32_t)delta);
    if(telnet_enabled)(void)holly_tcp_stream_tick(&guest_stream,(uint32_t)delta);
    if(http_enabled)(void)holly_tcp_stream_tick(&http_stream,(uint32_t)delta);
    last_tick = us;
    if (wiki_active)
      (void)holly_client_tick(&outbound, (unsigned)delta);
  }
  web_poll();
}
int holly_pi_network_expression(uint64_t us,
                                enum holly_expression *expression) {
  if (!expression || holly_pi_network_status != 2 ||
      stream.ssh.stage != HOLLY_SSH_RUNNING)
    return 0;
  if (stream.ssh.chat.turns != displayed_turn) {
    displayed_turn = stream.ssh.chat.turns;
    last_turn_time = us;
  }
  *expression = last_turn_time && us - last_turn_time < 900000u
                    ? stream.ssh.chat.expression
                    : HOLLY_LISTENING;
  return 1;
}
#else
unsigned holly_pi_network_restarts;
uint8_t holly_pi_network_ip[4];
int holly_pi_network_dhcp;
int holly_pi_network_status;
int holly_pi_network_start(void) {
  holly_pi_network_status = 0;
  return 0;
}
void holly_pi_network_poll(uint64_t us) { (void)us; }
int holly_pi_network_expression(uint64_t us,
                                enum holly_expression *expression) {
  (void)us;
  (void)expression;
  return 0;
}
#endif
