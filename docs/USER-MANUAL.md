# Holly AI Learning OS v0.49.29

## 1. Welcome aboard

Holly AI Learning OS v0.49.29
New User and Developer Manual - 5 October 2026
By Darren "Viper" Crawford - JMC Publishing

Holly is a Raspberry Pi 4 fan project inspired by the Red Dwarf ship computer. It boots its own independently authored ARM64 kernel rather than Raspberry Pi OS or Linux. You can talk through an SSH session, a simple Telnet session, or the web page hosted by the Pi. HDMI shows a full-screen portrait. Browser speech can animate both the web portrait and the Pi's HDMI face.

This manual covers the fresh Lite installation and the current source release. Lite means an empty, compact installation, not a reduced feature set. It includes the same v0.49.29 kernel features and built-in reference data as the update release. Uploaded books, episode transcripts, notes and trained changes from another owner's SD card are not included.

What Holly can do: character and episode discussion within its supplied knowledge, searching uploaded plain-text documents, remembering reviewed facts, exact question-and-answer lessons, notes, time/date, alarms, arithmetic, current headlines and online topic summaries. Web chat can switch between Holly, Hilly and Queeg.

What to expect: this is experimental fan software with authored replies, bounded retrieval and small original neural models. It is not ChatGPT-level reasoning, an actor's voice clone, a complete operating-system desktop, or a guarantee of knowing every episode. Reading a file does not retrain the conversational model. Online results can be wrong or unavailable.

Start with chapters 2-4. Use chapters 6-9 to add knowledge. Chapters 12-15 cover maintenance and development. The README and release notes describe the exact package and validation scope.

## 2. Hardware and installation

You need a Raspberry Pi 4, a suitable power supply, a microSD card, wired Ethernet to a router with DHCP, and a computer or phone on that LAN. HDMI is optional for network chat. The release targets Pi 4; Pi 3, Pi 5, Wi-Fi, USB audio and a general desktop are not supported by this package.

The complete raw image occupies 268,435,456 bytes (256 MiB). Use a card with more actual capacity than this; 512 MB or larger is a practical minimum. Common 8, 16, 32, 64, 128 and 256 GB cards use the same image. Storage remains finite, and very small cards hold fewer documents. On first boot, the marked Holly data partition expands to the card's detected capacity. Do not move its start sector with a generic partition editor.

Fresh installation:
1. Download and extract Holly-AI-Learning-OS-v0.49.29-Lite-Pi4.zip.
2. In Raspberry Pi Imager choose Use custom and select the extracted .img.
3. Select the destination card and write it. This replaces that card's contents.
4. Imager's Linux username, Wi-Fi and SSH customisation does not configure Holly. Use wired Ethernet and Holly's supplied login.
5. Insert the card, connect Ethernet and optional HDMI, then power on.
6. Look in your router's DHCP client list for the new Pi and record its IP address. The manual uses PI_IP as a placeholder; replace it in every command.

The source ZIP is for building and editing, not for flashing. The update kernel is for an existing installation. Use the complete Lite .img for a new card.

The boot partition contains official Raspberry Pi firmware plus Holly's kernel8.img and config.txt. The second partition is a custom Holly data area, not a Windows or Linux filesystem. Cancel Windows offers to format unrecognised partitions. A Pi HDMI picture alone does not prove networking is ready; allow time for boot and DHCP.

## 3. First login and storage setup

On Windows open Command Prompt or PowerShell. On Linux or macOS open a terminal. Run:

```
ssh -o ConnectTimeout=60 -o RekeyLimit=4M Rimmer@PI_IP
```

Username: Rimmer. Password: smeghead. Capitalisation matters. No characters appear while you type the password. At the first host-key prompt, compare the fingerprint with the release's published fingerprint where available, then accept the expected device. A different source build has a new host key. DHCP may assign a new IP after reboot.

Holly displays holly> after authentication. This is a chat/administration prompt, not a Linux shell. ls, apt, sudo and arbitrary shell commands do not work. Enter:

```
version
storage
storagediag
train status
```

Version should identify v0.49.29. Storage should say Holly Vault online. If storage is RAM-only or unavailable, diagnose that before expecting anything to survive reboot.

On a NEW, EMPTY Lite card, initialise its document bank once:

```
doc format
doc status
doc stats
```

Formatting discards document-bank contents. Never use it as a routine repair on an existing installation with uploads you want to keep. An unavailable bank on a fresh card is expected before this step; on a previously populated card it needs investigation instead.

The Vault saves lessons, reviewed memories, history and training state. The document bank holds uploaded text and indexes. Its capacity/catalogue is sized to the card within implementation limits. A dedicated web-reference area uses one document slot after initialisation.

For later sessions, check status rather than formatting again. Keep your original text files on your computer as backups. Exit an SSH conversation with exit. Prefer quiet, completed writes before removing power; abrupt removal during a write can still damage storage.

## 4. Web and Telnet conversation

HTTP and Telnet start automatically after boot. In Chrome open http://PI_IP/. The Pi hosts the page; no separate chat application is required for typed conversation. Type in the bottom text box and press Send. New chat resets that browser conversation's context; it does not erase stored documents or the Vault. The version appears at the top right.

The face occupies most of the screen. A thinking/search progress indicator shows ongoing work and elapsed time. During an online task, wait for completion rather than repeatedly pressing Send. A network task can fail or time out; it cannot always produce a usable answer.

Try these prompts:

```
hello
Tell me about Lister
Tell me about season 2
Tell me more
What is 10 plus 10?
NEWS
```

Broad season replies depend on authored coverage; episode detail depends on available references/transcripts. A title in script list establishes an uploaded title, not complete understanding of its plot. Chapter 7 explains a more precise script question.

Web and Telnet are guest conversation interfaces. SSH is used for administration, uploads, reviewed personal memories and training. Guest sessions are not separate private user accounts. Ship notes and alarms are shared features, not per-user private notebooks.

Telnet is optional. Use a Telnet client to connect to PI_IP port 23. Windows may need its optional Telnet Client enabled. The commands telnet on/off/status and http on/off/status are entered over SSH. Switching a service off applies to this boot; both start on again after reboot.

This release uses HTTP on port 80, Telnet on 23 and SSH on 22. HTTP/Telnet are unencrypted LAN access. The demo password is shared by release installations; the downloadable binary also shares its embedded host identity. Keep these services on your trusted LAN. For an independently provisioned source build, chapter 14 explains creating a unique SSH key and password.

## 5. Voice, faces and HDMI

Browser speech uses voices available to Chrome and the operating system of the viewing device. Audio comes from that device; the Pi does not supply native speech audio in this release. Voice selection is not a recording or clone of the performers.

Persona triggers in web/voice chat:
- HOLLY: Holly portrait and a British male voice when available.
- HILLY, HILLARY or HILARY: Hilly portrait and a British female voice.
- QUEEG: Queeg portrait, an identified American male voice and stern replies.

The selected persona remains until another trigger changes it. Queeg orders people about and sometimes criticises Holly; he uses thinking wording instead of the Junior Encyclopedia label. SSH/Telnet remain Holly chat. Queeg prefers an explicitly male US voice or a known en-US male voice such as Microsoft David. Generic Google US English is not assumed male. If an identified US male voice is missing, a notice appears and captions remain available.

Microphone recognition requires browser support, permission and a secure origin. Ordinary http://PI_IP is not a secure origin in default Chrome. Being built into the page does not bypass Chrome's restriction. Typed chat still works. Where a compatible secure origin has been configured, allow the microphone, use Talk to Holly, and pause after speaking to submit the recognised phrase. The page requests continuous listening by default, but browser permission/autoplay policy may still require an initial click. Recognition can use a browser/vendor service; it is not guaranteed offline.

Do not assume any HTTPS reverse proxy will work unchanged. The current HTTP server validates numeric Host and HTTP Origin headers. A domain/HTTPS proxy needs compatible header handling, and successful certificate issuance is a separate proxy configuration issue. This release does not include an unrestricted domain-proxy fix.

HDMI displays a face, not the browser controls. Speech sends numbered mouth events to the Pi, with retries and a watchdog so stale events cannot stop a later utterance. Mouth movement is approximate speech timing, not phoneme-perfect lip sync. Other clients and lost connections can affect a shared display. For diagnosis over SSH use:

```
displaydiag
display 0
display white
display face
```

Port 1 can be selected with display 1; recovery history made its allocation opt-in. Inspect the current diagnostic output. Return to display face after colour tests. Changes to resolution/firmware may affect physical scanout even when framebuffer writes succeed.

## 6. Uploading your own plain text

Use the supplied tools/holly_upload.py on your computer. You need Python 3 and an installed OpenSSH client. The uploader itself uses the Python standard library; it does not require pip packages. It sends commands through Holly's SSH chat protocol. SFTP, SCP and a remote filesystem shell are not implemented.

Prepare a nonempty UTF-8 plain-text .txt file. Normalisation transliterates punctuation and other Unicode characters into Holly's supported ASCII text without changing the original file. Choose a clear title of 1-80 printable ASCII characters. Keep a copy of your source text.

Run from the source/release folder on Windows:

```
py -3 tools\holly_upload.py PI_IP MyBook.txt --title "My Book" --normalize --user Rimmer
```

On Linux or macOS:

```
python3 tools/holly_upload.py PI_IP ./MyBook.txt --title "My Book" --normalize --user Rimmer
```

Enter the SSH password when requested. First ensure doc status reports the bank online. If it is a brand-new empty card, initialise it using chapter 3. Do not format a populated bank because an upload failed.

The tool checks the server's limits, splits oversized files into titled parts when possible, transfers resumable chunks, and verifies the committed SHA256. Current transport sends up to four durable 4 KiB pages together. Progress printed every 65,536 bytes is a progress interval, not the wire chunk size. Custom SD and TCP processing can still be slow; this is not desktop SFTP speed.

If interrupted, rerun the exact same file/title/options. Verified upload offsets allow resumption. Modifying the text or title changes identity and can consume more slots. Read any reported error before starting another copy. Very large documents can exceed either per-document, catalogue or free-space limits.

After completion, reconnect over SSH and use:

```
doc list
doc status
doc stats
find a distinctive phrase
reading status
reading list
```

The uploader's successful commit establishes stored text, not trained neural weights. Reading progress and relevant questions are the next checks.

## 7. Episode transcripts and coverage

No copyrighted TV transcripts or novels are bundled in the fresh release. Owners can import their own text. The script importer can also privately fetch the linked transcript index used during development. That index provided 51 linked transcripts from series 1-8 during the earlier import; it did not prove every episode or every multipart section was present. Site availability and coverage can change.

For a local directory of one .txt per episode:

```
py -3 tools\holly_scripts.py PI_IP "D:\RedDwarfScripts" --normalize --user Rimmer
```

For the supplied site's linked collection:

```
py -3 tools\holly_scripts.py PI_IP --from-site --normalize --user Rimmer
```

Use --dry-run to validate/measure without writing to Holly. Use --collection "Extras" for an additional collection and retain that label on resume. An optional manifest is a JSON list of title/file records for explicit ordering. Run the tool with --help for all options. Keep source titles consistent and include all parts of multipart episodes that you intend to cover.

Over SSH check:

```
script coverage
script list
script ask The End | radiation leak
```

The vertical bar separates episode title from the question/keywords. It is required for script ask. Type script, not scipt. When an episode spans packed documents, script list may repeat its title with multiple document IDs. That is not necessarily a duplicate episode or corrupt upload.

Natural conversation can use references, but a precise episode title plus important words is often more effective than asking everything Holly knows. If season 1 works but later seasons do not, inspect coverage and titles, test an exact passage, and try a specific episode question. Uploading more text improves evidence coverage; it does not automatically create a fluent season-by-season summarising model.

A returned passage is context from a source. It may contain dialogue or character opinions rather than objective facts. Use the surrounding episode and source commands when an interpretation matters. Keep attribution and applicable permissions with text you redistribute; the release includes importer tools rather than a copy of your uploaded collection.

## 8. Reading books and using documents

Committed text enters a checkpointed background reading queue. This allows bounded retrieval/indexing and selected-document conversation. It is reference processing, not a self-improving large language model. Keep the Pi running and give it quiet time to process; constant commands can compete for resources.

Inspect and control reading:

```
reading status
reading list
reading select 1
reading pause
reading resume
reading clear
```

Replace 1 with a ready document ID shown by reading list. Clear removes the session's selected-document focus; it does not delete all files. Check the response/status when changing controls. A document split across parts may require selecting the relevant part and asking narrower questions.

For a novel such as Infinity Welcomes Careful Drivers, first find its exact uploaded title and document ID. Select that document in the same conversation where you ask questions. Use distinctive terms from the text. A broad prompt such as tell me everything about the novel can exceed the reader's ability to summarise. Try what happens to Lister in this part or find a named incident instead.

Useful checks:

```
find Lister
Tell me about the novel Infinity Welcomes Careful Drivers
source
```

Web and SSH keep separate conversation context. Selecting a book in SSH does not necessarily select it in an existing browser chat. In the web text box issue reading list / reading select ID as needed, start New chat to clear old context, and refresh with Ctrl+F5 after an update. It is possible for an exact SSH search to succeed while a broad web question has no selected source.

For biographies or quotes, write small structured files with clear headings and explicit Q/A pairs where appropriate. Separate story facts from your interpretation. A file titled Dave Lister Biography with concise facts and dates is easier to search than unlabelled paragraphs. Quote collections can be retrieved as references; they do not guarantee random use in every reply. Exact teach lessons, in chapter 9, are better for a response you specifically want Holly to produce.

All searches remain bounded. Large card capacity does not mean unlimited catalogues, RAM or search time. Retain your original collection and organise titles instead of relying on Holly as the only copy.

## 9. Memories and exact lessons

These tools are different from document upload. Use SSH for administration and reviewed personal facts. Web/Telnet guest sessions do not expose or edit the personal-memory journal.

Save a fact:

```
remember My name is Darren and my nickname is Viper
memory list
memory show 1
what is my name
```

Replace example ID 1 with the ID Holly returns. Remember creates a saved fact with source user and a confidence label. A label is stored metadata, not proof of truth. Existing automatic short-statement capture is limited; use remember when persistence matters. Chat model mode can pause automatic capture.

Review and correct:

```
memory find Darren
memory correct 1 => My name is Darren Crawford
memory confidence 1 100
memory forget 1
```

Forgetting removes that journal entry; it does not undo prior trained weight updates. Normal replies can recall relevant memories without printing the entire bank. Ambiguous facts or low confidence can affect recall. Keep important personal information out of a shared fan-group distribution.

For an exact preferred response, teach a question-and-answer lesson:

```
teach Who built you? => Darren Viper Crawford built this Holly fan computer.
```

Lessons use a bounded associative/exact reply mechanism. They are not general model training. A lesson for one wording may not cover every paraphrase, and the lesson store is finite. Use list to inspect lessons, help for commands and storage to confirm persistence.

To paste a hundred quotes or long biographies, put them in a .txt and upload rather than pasting a huge SSH line. Use explicit lessons for a few chosen interactions, reference files for larger material, and reviewed memories for concise facts. None of these operations makes unsupported claims automatically true.

## 10. Time, notes, alarms and arithmetic

The Pi needs a current clock after boot. Opening the web page synchronises it from the viewing computer. Alternatively, run the supplied clock helper from a computer with correct time and timezone:

```
py -3 tools\holly_clock.py PI_IP --user Rimmer
```

The SSH form is clock set UNIX_SECONDS OFFSET_MINUTES. The offset is minutes relative to UTC, such as -420 for UTC-7. It is a fixed offset, not a timezone database; reseed after offset changes or reboot. Correct clock time also matters for HTTPS certificate validation.

Ask what time is it, what is todays date, or clock status. If Holly reports no synchronised time, seed it instead of trusting a stale answer.

Ship notes can be entered naturally:

```
take a note: Upload my episode collection this weekend
recall my notes
recall note 1
```

If you say only take a note, Holly prompts for the next message. cancel note cancels that pending entry. Notes are limited to short text (keep below 256 characters). Latest recall shows a bounded selection; use the returned note ID for older entries. These are shared ship notes, not private browser-user accounts.

Alarm examples:

```
set an alarm in 5 minutes
set an alarm for 07:30
alarm status
```

A clock and working storage are required. Alarm records persist, but correct time must be restored after reboot. The browser polls for announcements and speaks the emergency-style line when speech is available. Keep the page connected and allow audio if you expect a spoken alarm. The Pi's HDMI portrait is not itself an audio speaker. This is a novelty reminder feature, not a critical alarm service. Inspect alarm status to see active/pending entries.

Arithmetic runs locally. Try what is 10 plus 10, 12 times 3 or 100 divided by 4. It is a bounded six-place calculator rather than a symbolic mathematics engine. Divide-by-zero, unsupported syntax and values outside its range may be rejected. Rephrase with a short expression when ordinary prose is misunderstood.

## 11. News, online lookup and cached topics

Say NEWS or read me todays world news. The Pi fetches BBC World RSS headlines over verified HTTPS. This is a recent-headline bulletin, not a promise that every item was published on your local calendar date. Browser chat polls for the finished result and can read it aloud. SSH/Telnet users ask news status after the fetch starts. news source reports provenance.

Unknown public-information topics can use a free online summary fallback. The Holly/Hilly interface calls it searching the Junior Encyclopedia of Space. Queeg uses thinking language. Technically, it uses DuckDuckGo Instant Answer summaries with Wikipedia Search/TextExtracts as a fallback. It is not Google's full search engine, arbitrary web browsing, or a general unattended crawler. No paid API key is bundled.

Controls:

```
search Saturn
search status
search source
search cache
search refresh Saturn
search forget Saturn
search on
search off
```

Refresh requests a new reference. Forget is SSH-only. Successful topic references can be cached persistently for up to 256 topics when the dedicated bank is available. Cache capacity is bounded; it does not silently become an unlimited neural memory. Source URLs and fetch time remain available even though ordinary answers omit an attribution preamble.

Local knowledge has priority. Personal requests, small talk and selected-book questions are handled differently from public search. A failed lookup returns an explanation; Holly cannot guarantee an answer to every unknown question. Service responses may be empty, rate-limited, oversized or unsuitable.

If a search works intermittently, check the clock, internet access, gateway/DNS and storage status. Wait for the single bounded outbound reader rather than starting many concurrent requests. Cached references can be stale: use refresh for time-sensitive facts. News is a separate fresh bulletin rather than part of Red Dwarf training.

Search terms leave your LAN for the providers. Their returned summaries are external information, not verified truth merely because the transfer used HTTPS. Use source when you want to check a result or preserve attribution with redistributed text.

## 12. Experimental neural training

Holly has original small dialogue models and a separate Seed-1 learning experiment. Document reading, reference caching, exact lessons and saved facts are not the same as neural training. No existing market AI framework or pretrained external AI weights are required to run the kernel; BearSSL and Pi firmware are external non-AI dependencies.

The Pi's train commands adapt Seed-1's output layer. They do not retrain Dialogue-4 or turn a library of novels into a ChatGPT-style model. Seed-1 has 39,986 parameters and is a limited character-completion experiment. Dialogue-4 has 1,266,121 integer parameters and uses compiled weights; its small corpus limits fluent generalisation.

First inspect train status. For a reviewed memory, approve its current revision into a split:

```
train approve 1 train
train approve 2 valid
train approve 3 test
train start 256
train status
```

Use actual, distinct memory IDs. Do not put the same text into training and evaluation. The dataset includes compiled seed records; there are bounded additional review slots. Approval takes a snapshot, so later memory edits require removal/reapproval. Wait for saved before further mutations and power changes.

Training runs a candidate, evaluates validation and retention gates, and promotes only a measured improvement. An accepted model is not proof of improved factual reasoning. Gate margins/accuracy measure the small character task, not conversational quality. Leave the Pi quiet between status checks so background work can proceed.

Other controls: train pause, train resume, train cancel, train checkpoint, train auto on, train auto off and model rollback. Automatic mode schedules rounds while idle using reviewed data; it does not crawl the web. Rollback restores a previous active model when one exists, not the document bank or memory journal.

SELF-TRAINING.md documents the experiment and host synchronisation protocol in detail. The source includes NumPy trainers and corpora. The conversational weights are present as generated C headers, but the release does not contain all original .npz optimiser checkpoints. You can build the shipped kernel without retraining; a new training run will produce new weights and needs its own evaluation.

## 13. Updates, backups and troubleshooting

For an existing compatible installation, a kernel-only update preserves data better than reflashing a fresh image. Shut down power after writes complete, place the card in your computer, back up its boot kernel8.img and config.txt, and replace only kernel8.img with the release update kernel. Preserve firmware, partitions and data. Reboot, confirm version, then Ctrl+F5 and New chat in Chrome. Do not run doc format unless deliberately starting an empty bank.

A complete card image backup captures every partition, including Holly's custom data. Power off and read the whole card with a disk-imaging tool, not just the boot files. Windows Win32 Disk Imager uses Read to create the backup; do not use Write when backing up. A raw clone needs a destination with enough actual bytes, even when both cards have the same advertised GB size. Keep a tested spare and the original uploaded text files. A fresh Lite image is not a backup of your knowledge.

Cannot connect: confirm the current router DHCP address, Ethernet cable, power supply and that you are on the same LAN. Ping does not prove SSH is healthy. In Windows run Test-NetConnection PI_IP -Port 22, then ssh -vvv -o ConnectTimeout=60 -o RekeyLimit=4M Rimmer@PI_IP. If TCP connects but key exchange stalls, retain that log and note whether other services respond. Avoid rapidly piling up reconnects; the custom SSH implementation handles one connection at a time.

Password denied: check Rimmer's capital R, password smeghead, target IP and installed release. A remembered host key can belong to a different device after DHCP reassignment. Confirm the device rather than blindly deleting warnings.

Storage unavailable: record storage, storagediag and train status. Do not assume online RAM means persistent storage. For a fresh empty card only, doc format creates the bank. For an existing bank, keep a full backup before investigating.

HDMI blank: record displaydiag, try display 0 / display white / display face, test the other connector with display 1, and check cable/monitor compatibility. A ready framebuffer or correct pixel readback does not prove visible scanout. An HDMI-disabled recovery kernel from the update release can help isolate networking; it is not the normal Lite display experience.

Web differs from SSH: refresh cached browser code, check version, start New chat and reselect the document in that web session. Voice missing: inspect available UK/US system voices and the browser permissions. Microphone blocked: check secure-origin requirements. Sync notice: stop speech, let the watchdog settle, reconnect/refresh and avoid competing browser speakers.

When reporting an issue, include version, card capacity, Pi model, connection type, relevant diagnostics and exact reproduction steps. Never post private generated SSH keys or a populated personal Vault backup.

## 14. Building from the source release

The GitHub source ZIP contains src/, tests/, tools/, model/, assets/, web/, upstream notices and pinned boot firmware. It excludes build products, personal data and generated private SSH credentials. Generated model/portrait/web headers are included so a kernel build does not require reconstructing every asset first.

Build on a Linux host or a Linux environment with Bash, make, host C compiler, Python 3, AArch64 GNU GCC/binutils, dosfstools and mtools. Provisioning additionally requires the Python cryptography package. NumPy is needed only for model training/tests. An example Debian/Ubuntu setup is:

```
sudo apt install build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
sudo apt install python3 python3-venv dosfstools mtools
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install cryptography
```

From the source root, build a network-capable kernel with a unique identity:

```
python3 tools/provision_ssh.py build --username Rimmer --public-demo
make -j4
python3 tools/verify_kernel.py --require-network --credentials build/ssh-credentials.bin
cp build/kernel8.img bootfiles/kernel8.img
python3 tools/make_sd_image.py
```

For --public-demo, enter smeghead twice. For a unique private password, omit --public-demo and use at least 12 characters. Provisioning refuses to overwrite existing credentials. The --ip option is the compiled fallback address; normal operation requests DHCP. A unique build fingerprint will differ from the published binary's key.

IMPORTANT: make clean deletes the entire build directory, including generated credentials. Keep a private backup or regenerate BEFORE rebuilding. An unprovisioned kernel can compile but does not include the live Pi LAN setup. Always run the require-network verification before packaging. Never commit build/ssh_credentials_generated.h or ssh-credentials.bin to GitHub.

The image builder writes only a regular file under dist/, checks the staged kernel matches the build, and validates MBR/FAT/readback. Flash that output as in chapter 2. The layout has an HLY2 marker and fixed data start at sector 458752; changing it requires corresponding kernel changes. Expansion uses MBR/32-bit sector fields; arbitrary multi-terabyte capacity is not claimed.

Firmware is pinned, included with notices, and is distinct from Holly's independently authored kernel. The source build contains BearSSL for TLS/X.509. Source availability does not automatically license Red Dwarf artwork, performer likenesses or other supplied media.

## 15. Development, tests and release checklist

Code map:
- src/boot.S, kernel.c, linker.ld: ARM64 startup and kernel layout.
- src/genet_live.c, dhcp.c, tcp_stream.c, pi_network.c: Ethernet and LAN services.
- src/ssh_server.c, ssh_kex.c, modexp.c: custom SSH transport/crypto integration.
- src/sdcard.c, vault.c, documents.c: storage, persistence, upload and reading.
- src/holly.c, lore.c, reference.c, assistant.inc: conversation, facts and helpers.
- src/dialogue*.c, language_model.c, selftrain.c: original inference/training.
- src/http_server.c plus web/index.html: HTTP routes and browser presentation.
- src/framebuffer.c, display.c, video.c: firmware framebuffer and animation.
- tools/export_*.py: regeneration of embedded web/art/reference assets.

Modify web/index.html then run python3 tools/export_http.py before rebuilding. For portraits use the supplied PNG assets and export_portraits.py; inspect exporter code before altering frame dimensions or order. The six Queeg inputs use the sixth frame for the final slot. Compiled portrait data uses lossless row compression to fit the reserved kernel footprint. verify_kernel.py enforces the ELF/image and memory bounds.

For model experiments, install the pinned model requirements into a suitable Python environment, then inspect trainer --help. Example:

```
python3 -m pip install -r model/requirements.txt
python3 model/dialogue/dialogue4.py train --steps 1200 --output build/dialogue4.npz
python3 model/dialogue/dialogue4.py export build/dialogue4.npz build/dialogue4_weights.h
```

Do not blindly replace production weight headers; compare the export interface with src/dialogue4.c and evaluate new output. Desktop training is optional; compiled headers build the current model without .npz files.

Useful build checks include make test, make access-test, make document-test, make guest-network-test, make video-test and make web-test. Additional targets in Makefile cover reading, arithmetic, search/cache, portraits and browser speech. Some tests need NumPy, host libraries, generated data or QEMU. A test cannot establish real HDMI scanout or prove the model converses naturally.

Release checklist:
1. Build from a clean, provisioned source checkout and verify live networking.
2. Run relevant host/browser/SSH tests and retain logs with their exact scope.
3. Generate/read back the complete image and publish SHA256 checksums.
4. Boot the fresh image on a physical Pi 4, initialise its empty document bank, test DHCP, SSH, web, HDMI, upload, reboot persistence and voice on a supported browser.
5. Inspect the public source archive for generated keys, personal data and stale build products.
6. Include README, release notes, manuals and upstream notices. Record unresolved limits.
7. Before making broad redistribution claims, choose a first-party source licence and establish permission for any supplied third-party fan media. No new open-source licence has been imposed by this documentation package.

The shipped Lite image passed MBR/FAT/boot-file/empty-Vault checks and simulated capacity-expansion checks from 512 MB through 512 GB. Existing v0.49.29 kernel/network identity and browser checks passed. The new Lite image's physical first-boot acceptance remains pending. This documentation release does not change kernel behaviour.

