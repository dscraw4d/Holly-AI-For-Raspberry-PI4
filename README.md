[![Downloads](https://img.shields.io/github/downloads/dscraw4d/Holly-AI-For-Raspberry-PI4/total?style=for-the-badge&color=brightgreen)](https://github.com/dscraw4d/Holly-AI-For-Raspberry-PI4/releases)
# Holly AI Learning OS

**v0.49.29 - Raspberry Pi 4 fan computer**  
By Darren "Viper" Crawford / JMC Publishing
## Support Holly OS

Holly OS is developed by Darren “Viper” Crawford. If you enjoy the project and would like to support its continued development, donations are appreciated.

[Donate via PayPal](https://www.paypal.com/donate/?hosted_button_id=K33RB4KTD8MW8)

Thank you for helping keep Holly’s computer senility under control.


Holly boots an independently authored ARM64 kernel and presents a Red Dwarf-inspired ship computer through SSH, a browser and HDMI. It does not run Raspberry Pi OS or Linux. Official Pi boot firmware and vendored BearSSL are included dependencies, not original Holly code.

## Start here

For a new card download **Holly-AI-Learning-OS-v0.49.29-Lite-Pi4.zip** from your release assets. Extract its complete `.img`, select **Use custom** in Raspberry Pi Imager, and write the destination card. Use a **Pi 4**, wired Ethernet and a card of **512 MB or larger**. The raw image is 256 MiB; the marked data partition expands to the actual card capacity on first boot. No 128 GB card is required.

Find its DHCP address in your router and replace `PI_IP` below:

```sh
ssh -o ConnectTimeout=60 -o RekeyLimit=4M Rimmer@PI_IP
```

Password: `smeghead`. On a **fresh empty installation only**:

```text
version
storage
doc format
doc status
```

`doc format` discards uploaded document-bank contents; do not repeat it on a populated card. Open `http://PI_IP/` for chat. HTTP 80 and Telnet 23 start automatically; SSH 22 is the administration/upload interface. The prompt is Holly chat, not a Linux shell.

## Features

- Built-in Red Dwarf references, authored personality, topic follow-ups and small original dialogue models.
- Full-screen HDMI portraits and browser speech animation for Holly, Hilly and Queeg.
- Web persona triggers: HOLLY; HILLY/HILLARY/HILARY; QUEEG. UK male/female voices for Holly/Hilly, identified US male voice for Queeg. SSH/Telnet stay Holly.
- Persistent Vault for memories, exact lessons, conversation history and experimental training state.
- SD-backed text uploads, resumable verified transfers, indexed document search and background reading.
- Episode script listing/coverage and `script ask The End | radiation leak`.
- Clock/date, shared ship notes, alarms, local arithmetic and BBC World headlines via `NEWS`.
- Free online topic summaries with a persistent 256-topic reference bank and provenance on request.
- Reviewed Seed-1 output-layer training, evaluation gates and rollback.

## Add knowledge

Install Python 3 and OpenSSH on your computer. Run from this source folder:

```sh
python3 tools/holly_upload.py PI_IP ./MyBook.txt --title "My Book" --normalize --user Rimmer
python3 tools/holly_scripts.py PI_IP ./EpisodeTexts --normalize --user Rimmer
```

Windows can use `py -3` instead of `python3`. Enter the password interactively. Repeating the same upload resumes verified chunks. Use `doc list`, `reading status`, `reading list`, `reading select ID` and `find phrase`. Files improve reference coverage; they do not automatically retrain Dialogue-4. No TV scripts, novels or another owner's uploaded knowledge are bundled.

## Manuals

- [Full user and developer manual](docs/USER-MANUAL.md)
- [Text manual](docs/Holly-OS-v0.49.29-User-Manual.txt)
- [PDF manual](docs/Holly-OS-v0.49.29-User-Manual.pdf)
- [Experimental training details](SELF-TRAINING.md)
- [Release notes](RELEASE-NOTES.md)
- [Validation scope](LITE-VALIDATION-v0.49.29.txt)
- [Historical notes](docs/HISTORICAL-README.md) - older limits are historical, not the current setup guide.

## Build from source

Linux build dependencies: Bash, GNU make, host C compiler, Python 3, AArch64 GCC/binutils, dosfstools and mtools. Provisioning needs Python `cryptography`; model training optionally uses `model/requirements.txt`.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install cryptography
python3 tools/provision_ssh.py build --username Rimmer --public-demo
# Enter smeghead twice for the public demo account.
make -j4
python3 tools/verify_kernel.py --require-network --credentials build/ssh-credentials.bin
cp build/kernel8.img bootfiles/kernel8.img
python3 tools/make_sd_image.py
```

For a private build, omit `--public-demo` and use a password of at least 12 characters. Provisioning makes a unique host key. `make clean` deletes `build/`, including credentials; preserve a private copy or provision again before building. An unprovisioned build can compile with networking disabled: always run the network verification gate.

Compiled model weights and generated portrait/web headers are supplied. Trainer sources/corpora are included, but original NumPy optimiser checkpoints are not. No external pretrained AI weights are needed. The image builder uses the included pinned firmware and writes a regular file under `dist/`; it never flashes a physical card.

## Source layout

| Directory | Purpose |
|---|---|
| `src/` | Kernel, original drivers, services, conversation, inference and generated headers |
| `tests/` | Host, browser, protocol and emulation checks |
| `tools/` | Upload, asset generation, provisioning and image packaging |
| `model/` | Original trainers, authored corpora and lore |
| `assets/`, `web/` | Portrait input frames and browser presentation |
| `bootfiles/` | Pinned Pi firmware, configuration and notices |
| `vendor/bearssl/` | Vendored upstream TLS/cryptographic source |
| `UPSTREAM-LICENSES/` | External licence texts and source revision records |

## Limits and validation

This is experimental fan software, not ChatGPT-level conversational intelligence. Retrieval, authored replies and small neural models have finite coverage. Automatic reading and topic caching are not general autonomous self-training. Pi `train` commands adapt Seed-1, not Dialogue-4.

Browser speech voices depend on the viewing device. Microphone recognition requires supported browser APIs, permission and a secure origin; ordinary LAN HTTP does not meet Chrome's default secure-origin requirement. Current numeric Host/HTTP Origin checks mean a domain HTTPS reverse proxy needs compatible header handling. Native Pi audio, Wi-Fi, SFTP, SCP and a general shell are not implemented.

The distributed demo login and binary host identity are shared. Keep the demo on a trusted LAN; independent source builds can provision unique credentials. Generated private credentials and personal Vaults are excluded from this source ZIP.

Lite image MBR/FAT/readback/empty-Vault and simulated expansion checks passed; v0.49.29 kernel/network and browser checks passed. **Physical first boot of the new Lite image remains pending.** See included validation records rather than assuming every hardware feature was tested.

## Attribution and licensing

An unofficial fan project; no affiliation or endorsement by the Red Dwarf rights holders or performers is claimed. JMC Publishing is project branding. No first-party open-source licence has been selected in this source release; see [LICENSE-STATUS.md](LICENSE-STATUS.md). Do not infer rights to supplied artwork, actor likenesses, transcripts or novels from source availability. Preserve BearSSL, firmware, device-tree and certificate notices in their directories.
