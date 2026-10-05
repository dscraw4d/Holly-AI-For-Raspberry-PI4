#ifndef HOLLY_SSH_SERVER_H
#define HOLLY_SSH_SERVER_H
#include "aes.h"
#include "holly.h"
#include "sha256.h"
#include "ssh_kex.h"
#include "ssh_packet.h"
/* Original, bounded SSH implementation. Transport supplies ordered bytes,
 * all-or-error writes, cryptographic randomness and provisioned credentials.
 * One authenticated Holly session; no filesystem shell, forwarding or SFTP.
 * Client-initiated rekey supported; 16 MiB hard limit per direction/key epoch. */
struct holly_ssh_credentials {
    uint8_t rsa_n[256],rsa_d[256],salt[32],password_hash[32];
    char username[33];
};
typedef int (*holly_ssh_write_fn)(const uint8_t *,size_t,void *);
typedef int (*holly_ssh_random_fn)(uint8_t *,size_t,void *);
enum holly_ssh_stage { HOLLY_SSH_IDENTIFICATION,HOLLY_SSH_KEX,HOLLY_SSH_DH,
    HOLLY_SSH_NEWKEYS,HOLLY_SSH_SERVICE,HOLLY_SSH_AUTH,HOLLY_SSH_CHANNEL,HOLLY_SSH_RUNNING,HOLLY_SSH_DEAD };
struct holly_ssh_server {
    enum holly_ssh_stage stage,resume_stage;
    unsigned have_session_id,key_exchanges;
    const struct holly_ssh_credentials *credentials;
    holly_ssh_write_fn write;
    holly_ssh_random_fn random;
    void *context;
    uint8_t incoming[HOLLY_SSH_PACKET_MAX+32], outgoing[HOLLY_SSH_PACKET_MAX+36];
    uint8_t scratch[HOLLY_SSH_PAYLOAD_MAX],mac_input[HOLLY_SSH_PACKET_MAX+4];
    unsigned used,total,encrypted_in,encrypted_out,header_ready;
    uint32_t rx_sequence,tx_sequence,rx_bytes,tx_bytes;
    struct holly_aes_ctr rx_cipher,tx_cipher,pending_rx_cipher;
    uint8_t pending_rx_mac[32];
    uint8_t rx_mac[32],tx_mac[32],session_id[32];
    uint8_t client_kex[4096],server_kex[2048];
    size_t client_kex_bytes,server_kex_bytes;
    char client_ident[256];unsigned ident_bytes,discard_guess;
    uint32_t peer_channel,peer_window,peer_max;
    unsigned channel_open,pty,started,auth_failures,close_pending,channel_closed;
    struct holly_session chat;
    char line[8256];unsigned line_bytes,discard_lf,line_overflow;
    uint8_t chat_output[8192];unsigned chat_used,chat_sent,chat_failed;
};
int holly_ssh_server_start(struct holly_ssh_server *,const struct holly_ssh_credentials *,
    holly_ssh_write_fn,holly_ssh_random_fn,void *);
/* Accepts any fragment/coalescing. Returns -1 on sticky failure, 0 otherwise. */
int holly_ssh_server_feed(struct holly_ssh_server *,const uint8_t *,size_t);
/* Explicitly erase per-connection secrets after disconnect or timeout. */
void holly_ssh_server_destroy(struct holly_ssh_server *);
#endif
