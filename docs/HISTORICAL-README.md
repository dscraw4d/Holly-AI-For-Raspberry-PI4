# v0.49.29 update

Queeg now selects an English US male browser voice, preferring an explicitly
male Google US voice or a known US male voice such as Microsoft David. His gruff
pitch remains. Holly/Hilly retain British male/female voices. If no identified
US male voice exists, captions appear with an explanatory notice.
See UPDATE-v0.49.29.txt and VALIDATION-v0.49.29.txt.

# v0.49.28 update

Adds Queeg using the six supplied portraits and a stern command persona in web
and voice chat. Mention QUEEG to switch; HOLLY or HILLY/HILLARY switches back.
Queeg uses thinking status, factual replies, orders and occasional Holly insults.
HDMI speech controls are numbered, retryable and protected from stale stops.
Lossless portrait row compression keeps all three avatars in the 16 MiB budget.
See UPDATE-v0.49.28.txt and VALIDATION-v0.49.28.txt for install and checks.

# v0.49.27 update

Unhandled public topics now enter automatic lookup regardless of question wording.
Fresh and cached replies speak the retrieved text without an unverified/storage
preamble. Personal requests and normal small talk remain local. A failed reader
returns an explanation, not an invented answer. Sources are retained on request.
See UPDATE-v0.49.27.txt and VALIDATION-v0.49.27.txt for install and checks.

# v0.49.26 update

Adds a basic local six-place calculator, a free Wikipedia article fallback and
an independent 256-topic persistent bank within one free document-bank slot.
`NEWS` fetches world headlines. See UPDATE-v0.49.26.txt for kernel-only install,
coverage limits and storage fallback; VALIDATION-v0.49.26.txt lists checks.

Online references retain their provider/source provenance. DuckDuckGo Instant
Answer is a topic summary API, not full web search; application identifier
`t=hollyos` is sent. The second provider is Wikipedia's Action API with Search
and TextExtracts. Wikipedia-derived text is attributed via stored source URLs
and the `search source` command; consult the source page for authors, history
and applicable CC BY-SA terms. No API key is packaged or needed.

## Current update: 0.49.25

See `UPDATE-v0.49.25.txt`. Successful online topic summaries automatically
persist in a bounded 64-topic Vault web-reference bank and can be recalled
without internet after reboot. Sources and UTC fetch-request time are retained;
references remain unverified and separate from personal facts/training. The
bank never silently evicts: use `search refresh <topic>` or SSH-only
`search forget <topic>`. `search cache` reports availability/count. Common
subject-request prefixes resolve to the same cached topic. This is persistent
reference retrieval, not neural retraining. Installation replaces only the
kernel and retains existing data. Holly now says “I'm searching the Junior
Encyclopedia of Space”; the browser uses the same in-character progress label.
Technical backend details and storage limits are in the update notes.

Earlier release notes below are historical.

## Current update: 0.49.24

See `UPDATE-v0.49.24.txt` for installation and limits. A kernel-only update
retains existing Vault and document data. Unknown public information questions
can use verified HTTPS DuckDuckGo Instant Answer topic summaries, with local
knowledge taking priority. `search <topic>`, `search status/source` and session
`search on/off` are available over SSH/Telnet. The browser polls and voices
its result, with “Searching the Junior Encyclopedia of Space...” in the meter.
This is a bounded topic-summary fallback, not full search results or automatic
neural training. Queries are sent to DuckDuckGo. Personal-term guards keep
common personal queries local, but are not exhaustive. Selected-book queries
stay grounded in the local file. The shared outbound reader remains bounded.

Earlier release notes below are historical.

## Current update: 0.49.23

See `UPDATE-v0.49.23.txt` for kernel-only installation and usage. Existing Vault
and document data are retained; no reformat is required. New TXT files and
existing documents enter a checkpointed background reading queue. Book-focused
conversation, explicit Q&A TXT references, persistent shared ship notes,
browser-synchronised clock/date, stored browser-announced alarms, and common
Kryten speech aliases are available across the existing chat transports.
This adds bounded reference retrieval, not generative model retraining.

Alarm announcements begin with Holly's emergency line, followed by the reminder.

Live news: say "Holly read me todays world news" for a Pi-fetched verified-HTTPS
BBC World RSS headline bulletin. Browser chat automatically reads the completed
result; SSH/Telnet users ask `news status`. Requires a synchronised clock and
DHCP internet access. News stays separate from Red Dwarf lessons/documents.

The web interface shows an indeterminate thinking/research bar with elapsed
seconds. News polling continues during speech; a stalled acknowledgement no
longer gates result retrieval. Background fetch errors show a reconnect notice.

Earlier release notes below are historical and may describe superseded behaviour.

## Current update: 0.49.15

See `UPDATE-v0.49.15.txt` for installation and document-bank reset instructions.
This release is a kernel update for existing Holly cards, not a full Pi Imager image.
Format 15 uses SD-backed documents, a geometry-sized catalogue, 4 KiB resumable
pipelined uploads and checksummed page indexes with bounded text/index caches.
`doc format` deliberately discards old documents; unrelated persistent data is
retained when the history boundary can safely be moved. Use the included tools.
The card and catalogue remain finite, and broad searches have a 32 MiB page budget.

Earlier implementation/release notes follow; storage sizing and package contents
below describe earlier versions unless explicitly stated otherwise.

Current update: **v0.49.10 — HTTP and Telnet start automatically**. See `UPDATE-v0.49.10.txt` for kernel-only installation.

Current update: **v0.49.9 — matching HDMI/web portraits and caption-style web chat**. See `UPDATE-v0.49.9.txt` for kernel-only installation.

# Current release: v0.49

Grounded character discussion, complete session resets and a live JMC HDMI status dashboard. The original Dialogue-4 weights are unchanged. See RELEASE-v0.49.md and FIRST-BOOT-v0.49.txt. Physical HDMI scanout, voice, a Norman Lovett portrait and convincing open-ended conversation remain unfinished.

# Historical release: v0.48

Original Dialogue-4: 1,266,121 integer parameters, position-weighted input/context encoders, 1,230 authored examples. It is selected by default when chat is enabled; model 1 and 3 remain selectable. See MODEL-v0.48.md. Neural fallback now works alongside the authored personality, and guest sessions preserve conversational context.

# Historical release: v0.47

Pi-only discussion mode: sourced open invitations, opinions, hypotheticals and follow-ups across SSH, Telnet and HTTP. See DISCUSSION-v0.47.md and FIRST-BOOT-v0.47.txt. This is a bounded conversational step; the tiny generative model is unchanged and cannot yet converse like a large language model.

# Historical release: v0.46

Pi-only Dialogue-3 weight traversal and Rimmer/smeghead SSH LAN demo. The Pi performs all inference locally. See PI-ONLY-v0.46.md and FIRST-BOOT-v0.46.txt.

# Historical release: v0.45

Original JMC boot splash is rendered on each firmware-enumerated Pi 4 display before Holly's face appears. The boot config explicitly requests two 640x480 HDMI outputs. See FIRST-BOOT-v0.45.txt and JMC-BOOT-v0.45.md. Physical HDMI output needs testing; SSH and Vault are retained.

# Historical release: v0.44

Persistent episode script packs from user-supplied text, with SSH batch import, episode list and cited passage search. The existing 2 MiB document bank limits how much can be loaded. No TV scripts are bundled. See SCRIPTS-v0.44.md and FIRST-BOOT-v0.44.txt.

# Historical release: v0.43

A new original 792,193 parameter Dialogue-3 model, with four-core matrix inference in the custom Pi 4 kernel. Full fresh-card image and kernel-only update. See FIRST-BOOT-v0.43.txt, MODEL-v0.43.md and VALIDATION-v0.43.txt. The default dry Holly personality and factual reference retrieval remain in place.

# Historical release: v0.42

Original deadpan Holly text personality, inspired by Norman Lovett's portrayal. Default in SSH, Telnet and web chat. Cited passages remain unchanged; smalltalk preserves topic context. Read PERSONALITY-v0.42.md and FIRST-BOOT-v0.42.txt. No new neural model or voice implementation is claimed.

# Current release: v0.41

New: cited answers from natural Red Dwarf questions and uploaded text, named-topic follow-ups, simple paired references, and uncertainty when a question has no matching passage. Read CONVERSATION-v0.41.md and FIRST-BOOT-v0.41.txt. This is an extractive retrieval improvement, not ChatGPT equivalence or a newly trained model.

# Current release: v0.40

See FIRST-BOOT-v0.40.txt for the complete fresh-card image and ACCESS-AND-DOCUMENTS.md for chat and upload commands. This release adds persistent document uploads, Telnet guest chat and an embedded HTTP chat server. It is an experimental release, not yet a complete conversational Red Dwarf expert. Historical notes below describe earlier milestones.

# Holly v0.39 — Red Dwarf reference and original ship-computer banter

Read `UPDATE-v0.39.txt` for installation and `RED-DWARF-KNOWLEDGE.md` for scope.
42 curated topics and 95 sourced replies are available offline over SSH.
Try `who is Lister`, `who plays him`, `dwarf Holly`, `tell me more`, and
`dwarf source`. This is authored reference retrieval, not a newly trained LLM.
The knowledge pack and original banter are enabled per session by default.
The existing experimental neural model is still enabled separately with `chat on`.

This update keeps the v0.38 HDMI controls and the v0.36 storage driver. Physical
v0.39 HDMI, network timing and voice have not been verified; voice is not yet
implemented. The historical release notes below retain their original status.

# Holly v0.38 — HDMI controls and colour tests

Kernel-only update for the working v0.36/v0.37 card. Read `UPDATE-v0.38.txt`.
Adds firmware display selection, explicit unblanking and viewport reset, opaque
pixels, RGB/BGR conversion, and SSH-controlled colour screens. `display bars`
keeps its test image visible until `display face` resumes animation. `displaydiag`
reports control results and framebuffer pixel readback. Secondary selection needs
successful firmware enumeration. These are repair candidates: physical HDMI
output has not yet been verified. QEMU cannot verify real Pi firmware or cables.

Storage worked on the user's physical v0.36 Pi; this update retains its SD/Vault
formats and SSH credentials. Dialogue-1 remains the limited experimental model
from v0.37; use `chat on`. No new model training capability is added in v0.38.

The notes below are historical release notes.

# Holly v0.37 — original Dialogue-1 conversation

Kernel-only update for the working v0.36 card. Read `UPDATE-v0.37.txt` and
`DIALOGUE-MODEL.md`. `chat on` enables an experimental 127,647-parameter word
model with one exchange of context. It was trained from random weights on a
small synthetic corpus; it supports narrow short exchanges and can misunderstand
out-of-scope prompts. Saved facts and taught replies take priority. The original
Seed-1 training/store formats and working SD startup are retained. Dialogue-1
uses its own desktop trainer and compiled weights; existing `train` commands
still update Seed-1. Physical v0.37 chat latency is unverified. HDMI remains black
on the user's Pi despite a ready framebuffer; no display repair is claimed.

The following earlier release notes describe the retained foundations.

# Holly v0.36 — Pi 4 storage bring-up and SSH diagnostics

Kernel-only repair candidate for the v0.35 card. Read `UPDATE-v0.36.txt`.
The user's v0.35 physical Pi boots with SSH, but reports RAM-only storage and
black HDMI. v0.36 corrects SD startup power/clock handling, adopts conservative
one-bit transfers, checks card errors and bounds, and fixes reserved HDMI request
fields. `version`, `storagediag`, and `displaydiag` now work over SSH. The physical
result remains unverified until the user installs this kernel. Preserve the old
kernel as a rollback copy; do not reflash the card.

The following v0.35 feature documentation remains applicable to the learning
engine; its earlier hardware status statements describe the pre-v0.36 release.

# Holly AI Learning OS 0.35 — self-training candidate

This release adds **persistent, resumable self-training for Holly Seed-1**. The Pi can adapt the output layer of its original neural model while idle, evaluate a separate candidate, promote improvements, and roll back. A reviewed dataset preserves sources and revisions. The desktop trainer resumes all-layer training with saved optimizer state, and `model/sync.py` transfers examples and weights over SSH without rebuilding the kernel. See `SELF-TRAINING.md` for the workflow.

**Seed-1 remains a tiny 39,986-parameter character model with mostly garbled output.** This release adds a tested training/update loop, not useful general conversation or ChatGPT-level intelligence. It retains the original architecture and supplied baseline weights.

The Pi can fetch a requested English Wikipedia summary directly over verified HTTPS and save a short attributed excerpt in the Vault. No companion computer fetcher is needed. Set certificate-validation time after boot, then use `wiki read Article Title` over SSH. The release includes a 256 MiB flashable Pi 4 SD image, `Holly-AI-Learning-OS-v0.35-Pi4-Self-Training.img`, and keeps the existing SSH identity. Holly's kernel and teaching engine are independently authored. TLS and X.509 use the vendored BearSSL 0.6 library; this is third-party cryptographic software, not an operating-system kernel or an AI model. Official Raspberry Pi VideoCore firmware, DTB and overlays load the ARM kernel. Their licenses and source information are included.

**SSH works in OpenSSH interoperability tests, including inside an emulated bare-metal Pi 4 kernel. The user confirmed DHCP and SSH on an earlier Holly release; the v0.29 storage migration and v0.35 learning changes have not yet been tried on the physical Pi.** The emulator does not implement GENET, RNG200 or SD storage, so it cannot verify hardware networking or the capacity expansion. HDMI startup also remains a physical check.

## Connecting when you choose to try it

Read `CONNECTION.txt` in the release ZIP for the unique password and host fingerprint. Keep this package private: the image contains its private host key and the package contains login credentials.

1. Extract the ZIP. In Raspberry Pi Imager choose **Use custom**, then the `.img`. Writing the selected microSD card erases it. Do not apply Raspberry Pi OS customization settings.
2. Insert the card into a Pi 4. Use its normal power supply. HDMI is optional.
3. Connect the Pi's Ethernet port to your normal router or LAN switch. Holly sends a DHCP request with hostname `holly` and waits for a lease. The router's DHCP client list should show the assigned address and Holly's Ethernet MAC. The kernel also prints the address with the serial `networkdiag` command. HDMI and serial are optional; the router lease list is the normal way to find the address.
4. From another computer on the same LAN, run `ssh -o RekeyLimit=4M holly@<DHCP-address>`. Replace `<DHCP-address>` with the address shown by your router. Check the displayed host fingerprint against `CONNECTION.txt`, then enter the password. DHCP renews the lease while Holly is running.
5. Try `hello`, `teach name of ship => Red Dwarf`, `ask name of ship`, and `repeat that`. Type `exit` to close the session. If no DHCP server answers, Holly falls back after about 30 seconds to `169.254.77.1` for a direct cable using `169.254.77.2/16`.
6. Say `The ship is Red Dwarf.` Holly saves this simple statement as unverified chat memory at 55% confidence. Try `ask ship Red Dwarf`, `why that?`, `that's right` or `that's wrong`, and `correct that => The ship is Blue Dwarf.` The feedback changes a persistent confidence label and can change which equally matching fact is preferred later. Use `learning off`, `learning on`, or `learning status` to control automatic fact capture for the current SSH session. `remember`, `memory list/find/show/correct/confidence/forget`, `storage`, and `history 5` remain available.
7. To verify persistence, save a fact and a lesson, power off without reflashing, reconnect, and check `memory show`, `ask`, and `history` again.

## Read Wikipedia directly from the Pi

The Pi 4 has no battery-backed clock. After each boot, get the current Unix time in Windows PowerShell with `[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()`. In your Holly SSH session, type `wiki time` followed by that number. This time advances using the Pi's monotonic timer. The reader refuses to start until time is set and a DHCP lease is active.

Then type:

```text
wiki read Red Dwarf
wiki status
```

`wiki read` starts an asynchronous request on the Pi; use `wiki status` to see its progress and final result. `wiki cancel` stops it. Holly uses the router's DHCP DNS/gateway settings, resolves `en.wikipedia.org`, verifies the TLS certificate chain, hostname and dates, and accepts a bounded JSON HTTP response. Failed validation or incomplete replies save no memory. Successful requests save up to 175 characters from the opening sentence with the article URL and a 45% unverified confidence label. Long openings are shortened with an ellipsis; unsupported Unicode is represented with question marks. Ask `Red Dwarf television`, `why that?`, or `memory list` to review it.

Titles currently support up to 48 ASCII letters, digits, spaces, underscores, hyphens and parentheses. There is one request at a time, a five-second minimum between starts, and a two-minute network timeout. Disambiguation pages, redirects, compressed bodies, and oversized responses are rejected. This reads requested summaries; it does not follow links autonomously or train a language model. If you redistribute excerpts, preserve the article link and follow the article's license terms. The older optional `wiki_reader.py` companion remains in the ZIP.

## Updating without reflashing

For v0.27 through v0.35 cards with the matching Holly Vault layout, replace only the FAT boot partition's `kernel8.img` using `UPDATE-INSTRUCTIONS.txt` in the release ZIP. Power off the Pi first, back up the whole card before its first migration, and keep a copy of the old kernel. Do not overwrite the MBR or the `0xDA` Vault partition; cancel any prompt to format the Vault. When upgrading directly from v0.27/v0.28, the v0.35 kernel migrates the old lesson journal on first boot and keeps the existing chat-history ring in place. A v0.26 card has no Vault partition and needs a one-time move to the persistent layout; that release kept lessons in RAM, so it has no saved lesson journal to preserve.

If a physical connection fails, it does not establish that the SSH protocol engine is broken; the board-specific Ethernet/RNG path has not been hardware-tested. Serial remains an optional diagnostic path, not a prerequisite for using SSH. No real-board success is claimed.

## What is implemented

- Independently authored freestanding kernel, memory runtime, PL011 diagnostics, monotonic timer, mailbox framebuffer and animated Holly face.
- Independently authored AES-128 CTR, SHA-256/HMAC, fixed 2048-bit modular exponentiation, DH group 14 key exchange and RSA SHA-256 host signatures implementing published standards.
- Encrypted SSH framing with integrity checks, password authentication, a single session channel, terminal request handling, interactive chat, client-requested key renewal and one-command execution. The session runs Holly commands, not a Unix shell.
- An original IPv4/TCP stream with checksums, sequence validation, bounded output queue, MSS/window handling, retransmission, duplicate suppression, zero-window recovery, link-loss reset and connection teardown. ARP and ping replies are included. No IP fragments or IPv6.
- An independently authored DHCP client with discover/request/ack, lease timers, renewal, rebinding, NAK recovery, ARP conflict probing and direct-cable fallback. It requests the hostname `holly` so common router lease tables can identify the board.
- Pi 4 GENET polling driver with private coherent DMA buffers, hardware-index ownership, PHY negotiation and bounded frame validation. RNG200 polling rejects faults, timeouts and repeated words. Board behavior is unverified.
- HDMI framebuffer startup now forces both Pi 4 micro-HDMI outputs to a known 720p mode when a display does not provide EDID; SSH chat selects Holly's HDMI expressions in the production integration.
- Original outbound IPv4 client with subnet/gateway ARP routing, UDP DNS, TCP active open, bounded receive/send queues, retransmission, duplicate suppression, partial acknowledgements and zero-window recovery. It runs alongside the inbound SSH session.
- TLS 1.2 using BearSSL 0.6, ECDHE with AES-128-GCM or ChaCha20-Poly1305, RNG200 entropy, and a pinned Mozilla root-store snapshot. Hostname, chain, key size and certificate dates are checked before HTTP application data is accepted. There is no certificate-revocation lookup; there is no TLS 1.3 or insecure fallback. See `HTTPS-IMPLEMENTATION.md`.
- A small original rule/associative teaching engine for ordinary chat. Up to 32 exact question-and-answer lessons remain supported. A separate reviewable memory journal stores up to 1,023 facts, each with a source, confidence score and revision. It supports list, keyword search, show, correction, confidence edits and forgetting. Chat recognises short statements using a narrow subject–verb–object grammar and saves them with source `chat statement` and a 55% unverified confidence label. A user can rate the last recalled fact with `that's right` (+20 points) or `that's wrong` (−30 points) and correct its text. Facts below 20% remain reviewable but are excluded from automatic recall. Lexical ties remain visible unless one confidence label leads by at least 30 points; the alternative ID is still shown. Two verified copies protect each update from an interrupted sector write. Chat turns remain in a checksummed rotating log. The direct reader adds one attributed Wikipedia excerpt at 45% unverified confidence per request. Holly has no autonomous web crawler. Seed-1 is a separate experimental trained model; ordinary chat still uses this retrieval engine. Reviewed facts can separately be approved for neural training.

## Limits

One SSH connection at a time; password authentication only; no SFTP, filesystem shell, forwarding or compression. Use `-o RekeyLimit=4M` so the client requests fresh keys before the 16 MiB per-direction key-epoch limit. Server-initiated renewal is unfinished; without timely client renewal the connection closes at the limit. The TCP sender uses one outstanding segment, so throughput is limited. Authenticated chat has a one-hour idle timeout. Crypto and drivers have not had an independent security/side-channel audit. The RNG checks do not certify entropy quality. The PHY setup assumes board firmware/reset defaults supply appropriate RGMII delay configuration; this needs physical verification.

### Persistence

The image begins with a 223 MiB FAT32 boot partition and a marked 32 MiB `0xDA` Holly Vault partition. At first boot, Holly reads the SD card's reported capacity and expands only this marked partition to the card's remaining capacity; on a 128 GB card the Vault therefore gets almost all of the card. Holly's SDHCI/eMMC2 driver reads and writes the Vault directly; it does not mount Linux, ext4 or another existing operating-system filesystem. On upgrade, the v1 lesson journal is copied into two alternating v2 snapshot pages before the old pages are reused for up to 1,023 source-tagged memories. The chat-history ring keeps its existing sector range. Memory facts are up to 256 bytes; sources are up to 96 bytes. Confidence is a user-supplied 0–100 label, not a calibrated probability. `memory forget` clears Holly's two logical record sectors, but SD-card wear levelling cannot guarantee physical sanitization of remapped flash cells. The original utterance may remain in rotating `history` until that ring wraps. Re-flashing still resets the Vault.

## Rebuilding

On a development computer install an AArch64 GCC/binutils cross-toolchain, `make`, Python 3, `cryptography` for host key provisioning/reference tests, and `dosfstools`/`mtools` for SD image construction. The builder can also use Zig 0.16 with `TOOLCHAIN=zig` and `pyfatfs` with `setuptools<81` when the native FAT utilities are unavailable. This v0.35 image used that alternate build path; its FAT32 boot structure and kernel cluster chain were also checked independently. These host tools are not OS runtime dependencies.

```sh
python3 tools/provision_ssh.py my-credentials --username holly --ip 169.254.77.1
bash build-ssh-image.sh my-credentials
make test
make ssh-test network-test
```

For Zig, replace the build command with `TOOLCHAIN=zig ZIG=/path/to/zig bash build-ssh-image.sh my-credentials`. Install `pyfatfs` and `setuptools<81` in the Python environment if `mkfs.fat`, `mcopy`, `mmd`, and `fsck.fat` are absent. Run `python3 tools/verify_boot_fat.py` to check the resulting image's FAT mirrors and `kernel8.img` cluster chain.

Provisioning reads your password interactively and creates a unique RSA host key. It refuses to overwrite credentials. This update ZIP does not include private credentials. Provision your own identity when building independently. A rebuild with new credentials changes the host fingerprint. To retain this image's identity, pass the extracted `credentials/` directory to `build-ssh-image.sh`. `build.sh` produces a diagnostic build with SSH networking disabled; use `build-ssh-image.sh` for an enabled image.

`tests/ssh_arm_kernel.c` and both host adapters are test-only. Their UART framing/host entropy never appear in the production image. QEMU itself is not packaged or linked into Holly. See `VALIDATION.md`, `SSH-IMPLEMENTATION.md` and `PI4-BENCH-TEST.md` for evidence and boundaries.

The firmware is pinned to `dcca4969e53d2a25e688f0f228a09486786d54c0`. The image builder operates on regular files, validates both MBR partitions and FAT32, and reads every boot file back from the finished disk image. `SHA256SUMS.txt` contains image and kernel hashes.
