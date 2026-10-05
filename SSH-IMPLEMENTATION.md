# Original SSH implementation and verification boundary

Holly implements published network/cryptographic standards with independently authored C. No OpenSSH, Dropbear, mbedTLS, OpenSSL, Linux network stack or existing AI runtime is linked into the kernel. Standards interoperability is necessary for a normal SSH client; originality here refers to the implementation, not inventing an incompatible protocol or cipher.

Supported suite: `diffie-hellman-group14-sha256`, `rsa-sha2-256`, `aes128-ctr`, `hmac-sha2-256`, and `none` compression. Password authentication runs only after encrypted NEWKEYS. Host identity is a provisioned 2048-bit RSA key. Each DH exponent, KEX cookie, padding byte and TCP initial sequence number comes from a random callback; production supplies RNG200, tests supply the host OS's cryptographic random source. There is no deterministic production fallback.

The implementation validates positive DH values, derives separate directional keys, verifies its own RSA signature, handles segmented/coalesced packets, authenticates encrypted packets before dispatch, limits authentication failures and explicitly wipes a destroyed session. The RSA exponentiation computes both candidate products for every secret bit; AES computes its substitution algebraically without secret-indexed lookup tables. These measures are not a proof of constant-time behavior or independent security approval.

The SSH session recognizes a terminal request, interactive shell request and exec request. Its shell is a Holly chat command processor. It respects client channel windows, buffers bounded output, reports exit status, and performs channel EOF/close. Unsupported forwarding and global requests are rejected. Client-requested rekey retains authentication, channel state and chat context. The first exchange hash remains the session identifier; fresh exchange hashes derive each new set of keys. NEWKEYS is authenticated with the old directional key before switching to its replacement. Packet sequence numbers continue across exchanges, and staging secrets are erased. Each direction has a 16 MiB key-epoch limit. Server-initiated rekey is unfinished; the release connection command requests client renewal at 4 MiB. No `none` authentication or plaintext fallback grants access.

The original TCP code on port 22 is distinct from the earlier offline diagnostic port-2222 module. It sends a single outstanding data segment, retains unacknowledged data, handles partial ACKs, retries, and ignores duplicate/out-of-order payloads while returning the current ACK. Zero-window persist probes retain the unacknowledged byte; window recovery resumes sending. A physical link-down transition destroys old connection state so a new login does not wait for the old socket to time out. It does not implement TCP congestion control for general Internet use, SACK, out-of-order reassembly or window scaling. This release targets a direct local Ethernet connection.

Hardware offsets and controller behavior were researched from primary hardware/project sources. Those facts inform independently authored drivers; their implementations are not copied or linked into Holly. The Pi driver presently preserves the PHY delay defaults rather than implementing all Broadcom PHY model-specific configuration. This is a material remaining hardware uncertainty.

Standards and primary references:

- [SSH transport, RFC 4253](https://www.rfc-editor.org/rfc/rfc4253.html)
- [SSH authentication, RFC 4252](https://www.rfc-editor.org/rfc/rfc4252.html)
- [SSH channels, RFC 4254](https://www.rfc-editor.org/rfc/rfc4254.html)
- [SHA-256 DH groups, RFC 8268](https://www.rfc-editor.org/rfc/rfc8268.html)
- [DH prime group 14, RFC 3526 section 3](https://www.rfc-editor.org/rfc/rfc3526.html)
- [RSA SHA-2 host signatures, RFC 8332](https://www.rfc-editor.org/rfc/rfc8332.html)
- [RSA encoding, RFC 8017](https://www.rfc-editor.org/rfc/rfc8017.html)
- [AES CTR in SSH, RFC 4344](https://www.rfc-editor.org/rfc/rfc4344.html)
- [SSH SHA-2 MACs, RFC 6668](https://www.rfc-editor.org/rfc/rfc6668.html)
- [AES, NIST FIPS 197](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf)
- [GENET hardware register definitions](https://github.com/torvalds/linux/blob/master/drivers/net/ethernet/broadcom/genet/bcmgenet.h)
- [Pi 4 GENET hardware bring-up reference](https://qemu.googlesource.com/u-boot/+/refs/tags/v2021.01-rc5/drivers/net/bcmgenet.c)
- [RNG200 hardware interface reference](https://github.com/torvalds/linux/blob/master/drivers/char/hw_random/iproc-rng200.c)
- [QEMU Pi 4 board implementation](https://gitlab.com/qemu-project/qemu/-/blob/v9.2.4/hw/arm/raspi4b.c)

The OpenSSH client and Python reference cryptography library are external test/provisioning tools only. Raspberry Pi's upstream boot firmware/DTB/overlay remain separate licensed boot assets. This project does not claim to have independently authored those assets.

Exception model reference: [Arm AArch64 exception model](https://documentation-service.arm.com/static/67ac57fb091bfc3e0a9479cc). The original vector table and guarded probe recover only a matching synchronous MMIO-load fault; interrupts and unrelated faults halt.

## HDMI startup

The shipped boot configuration enables both Pi 4 micro-HDMI connectors, selects a 720p60 fallback mode, disables overscan and ignores framebuffer alpha. This keeps the firmware display active when an attached monitor does not provide usable EDID; the kernel then requests and renders a 32-bit framebuffer through the mailbox interface.

## Persistent Holly Vault

The kernel initializes the Pi 4 eMMC2 SDHCI host, decodes the SD CSD capacity, and expands the data partition only when the MBR identifies Holly's marked image layout. During a direct v0.27/v0.28 to v0.33 upgrade, the latest v1 lesson snapshot is committed to a v2 page before pages 2–63 are reused for memory. Lessons then alternate across two snapshot pages. The memory store has 1,023 fixed slots, each with two CRC-checked sectors for updates; each item carries text, source, confidence and revision. Forget commits a tombstone before clearing the prior logical copy. The chat history keeps its prior sector range and alternating metadata sectors, so the migration does not move that log. `memory list/find/show/correct/confidence/forget` manages facts; `memory find` and natural chat recall use lexical token overlap, not vector or neural semantics. A session can inspect its last recalled fact with `why that?`, update it with `correct that => fact`, or rate it with `that's right` and `that's wrong`. Short declarative statements become unverified, source-tagged memories by default; `learning off` suppresses automatic capture for the current session. Low-confidence facts remain reviewable but are excluded from automatic recall. `storage` reports capacity and counts; `history [1-10]` retrieves recent turns. The migration, sector layout and physical SD writes still need Pi 4 testing.

## DHCP and LAN addressing

The production integration starts an independently authored IPv4 DHCP client when GENET reports link up. It sends DHCPDISCOVER/DHCPREQUEST with hostname `holly`, validates transaction ID, client hardware address, server identifier, lease, mask and UDP checksum, then probes the offered address with ARP before enabling TCP/SSH. It renews at the server's T1, broadcasts at T2, handles NAK/expiry, and sends DHCPDECLINE when another host claims the offered address. If no lease is acquired after about 30 seconds, the configured 169.254.77.1 address remains available for a direct-cable fallback. The router's DHCP client list is the intended way to discover the assigned LAN address.

The v0.33 production network loop also polls a separate outbound DNS/TCP client. `wiki read` schedules a direct HTTPS job without closing SSH; `wiki status` reports its result. See `HTTPS-IMPLEMENTATION.md` for TLS library attribution, certificate/time checks and hardware validation boundaries.
