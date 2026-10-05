# Holly v0.35 — self-training loop

This release implements reviewed example collection, persistent model snapshots,
resumable training, background Pi updates, evaluation before activation,
automatic promotion, and rollback. Model updates no longer require rebuilding
or replacing the kernel after this one-time v0.35 upgrade.

**The Pi trains Seed-1's output layer; the desktop trainer can train all layers.**
The architecture is still the small, original 39,986-parameter character model.
Most generated text remains garbled. These mechanisms do not make it a capable
conversational AI or guarantee that its statements are true.

## First use over SSH

Install the v0.35 `kernel8.img` using `UPDATE-INSTRUCTIONS.txt`, then reconnect
with the same SSH login. Do not reflash the full image to preserve existing data.

```
train status
memory list
memory show 1
```

`train status` should eventually report `saved`, not `RAM ONLY` or `SAVE FAILED`.
Initialization and checkpoints take time; wait for `saved` before the next
mutation command or powering down. Network access and model inference remain
available during background checkpoint writes.

After reviewing a memory's text and source, approve its current revision:

```
train approve 1 train
```

Replace `1` with the actual memory ID. Wikipedia imports, explicit corrections,
and saved conversation statements all use the same approval path. Approval is a
snapshot: later edits or forgetting a memory do not silently change that snapshot.
To replace it, use `train remove ID`, wait for `saved`, then approve it again.
Removing a record does not undo weight updates already learned from it.

Different reviewed memories can be reserved for evaluation:

```
train approve 2 valid
train approve 3 test
```

Use distinct texts and IDs. Do not copy a training example into an evaluation
split. Duplicate text is rejected after case/whitespace normalization, and a
memory ID cannot appear twice. This version holds 128 records: 54 compiled seed
records plus up to 74 approved memories, each up to 256 characters. It does not
train on whole Wikipedia articles. The compiled corpus also includes fixed replay,
validation, and retention examples, so a first training run does not require
creating every split yourself. Neural output and raw conversation history are
never automatically harvested as training targets.

Start a bounded trial:

```
train start 256
```

It trains a separate candidate, evaluates it, and either activates it or discards
it. Use `train status` occasionally; repeatedly issuing commands keeps training
idle. Leave at least a few seconds between checks or disconnect SSH while it
works. Counts in the status show progress, accepted/rejected candidates, and
model generation. Expect rounds to take minutes on the Pi; hardware timing has
not yet been measured.

To keep running rounds automatically while idle:

```
train auto on
```

Automatic mode persists across reboot. It starts 2,048-update rounds after a
60-second scheduling delay and rests for 60 seconds between completed
rounds. Training/evaluation wait until no Holly command has arrived for two
seconds. Automatic mode is off in a fresh installation until you enable it.
It uses the reviewed dataset, not an unattended web crawler.

## Controls

`learning off` controls the older chat fact-capture feature. Use `train pause`
or `train auto off` to control neural training.

| Command | Effect |
| --- | --- |
| `train status` | Phase, pause/auto flags, generation, steps, record count, gates, persistence status |
| `train pause` | Keep candidate and position, pause computation, checkpoint |
| `train resume` | Resume the current candidate or allow automatic scheduling |
| `train auto off` | Stop scheduling new rounds; a current round can finish |
| `train cancel` | Discard current work, stop automatic mode, keep active model |
| `train checkpoint` | Queue an explicit checkpoint of current progress |
| `train remove ID` | Remove an approved memory snapshot; compiled seed records remain |
| `train export INDEX` | Export one approved record with source, ID, revision, split, and text |
| `model rollback` | Swap active and previous model; stop automatic scheduling |
| `model generate TEXT` | Generate with the active model |

Status phases: `0` idle, `1` training, `2` evaluating, `3` receiving an upload.
Most mutation commands refuse while a checkpoint is being saved. Dataset edits
and rollback require an idle candidate; cancel and wait for `saved` first.
Rollback retains one previous active model. It is unavailable before the first
successful promotion. Rollback changes weights, not your fact journal.

## What is trained and what is measured

The Pi freezes the learned embeddings and hidden layer and adapts the 6,272
output weights plus 98 output biases. It uses original integer stochastic
subgradient updates on a multiclass margin loss, with bounded parameters and a
fixed small step. There is no floating-point library or AI framework in the
kernel. The replay cursor cycles through old and newly approved training records
across rounds, reducing the risk of replacing all old examples with new ones.
This is output-layer adaptation, not full on-Pi backpropagation.

Each candidate and the active model are evaluated separately on:

1. Validation records (split `valid`).
2. Retention/test records (split `test`).
3. Eight fixed old training records, used as additional retention anchors.

Promotion requires **strictly lower total validation margin loss**, no higher
margin loss on either retention group, and no lower next-character accuracy in
any group. Identical weights fail the strict-improvement test. Uploaded desktop
models pass the same gate; successful transfer does not mean activation.

`Gate 0/1` are active/candidate validation, `2/3` retention/test, and `4/5` old
anchors. Margin values use the integer scale 4,096 and are summed over tokens;
compare each pair only when token counts match. They are not cross-entropy,
confidence percentages, or factual-accuracy scores.

These small operational evaluation sets are reused across rounds. They are not
an untouched independent benchmark after repeated candidate selection and can
be overfit indirectly. There are no broad reasoning, factual correctness, or
conversation-quality guarantees. Add distinct reviewed evaluation examples and
retain an external benchmark when developing larger models.

A host experiment ran 12 real 64-update rounds from the supplied weights:
2 candidates passed and 10 were rejected. The first accepted candidate reduced
validation margin from 3,866,611 to 3,856,095 and retention margin from 3,357,423
to 3,348,255 without reducing next-character accuracy. These are small measured
prediction improvements, not evidence of useful dialogue. The release still
ships the original baseline weights; it does not silently install those
experimental candidates.

## Persistent storage and power loss

Holly's existing Vault allocates its remaining partition space to conversation
history. This release therefore stores model state in the previously empty gap
before the FAT boot partition, **without shrinking or moving the Vault**.

The loader requires Holly's MBR marker, exact boot-partition layout, expected
Vault start, no extra partitions, and either its own model-store marker or an
entirely empty target region. It refuses unfamiliar data. The marker is at LBA
8; alternating checkpoint banks begin at LBA 9 and 1028. No writes occur outside
LBA 8–2046. The boot partition still starts at LBA 2048. A physical SD test is
still required; make a whole-card backup before first use of the new storage.

A checkpoint includes active/previous/candidate weights, reviewed data, flags,
training cursor, update count, and evaluation state. Inactive-bank payload
sectors are written and read back before a CRC-checked header is committed.
The previous committed bank remains the recovery point on an interrupted write.
Inference keeps using the old model until the promotion checkpoint completes.
The loader checks payload CRCs and falls back if the newer bank is corrupt.
A torn very first marker is deliberately refused; it is not guessed to be safe
or automatically overwritten. If both banks are invalid, the built-in model
is the fallback and automatic training remains off.

Computation is split into at most one token operation per eligible 20 ms timer
interval. Checkpoint I/O is split into one sector write/readback per eligible
2 ms interval. Actual SD latency can exceed these intervals; this is cooperative
scheduling, not a hard real-time guarantee. Network polling happens first.
Physical Pi throughput and SSH latency during training remain unmeasured.

Progress is checkpointed every 256 Pi training updates and at evaluation phase
boundaries. A sudden power cut may lose work since the last completed checkpoint.
Pause or request a checkpoint and wait for `saved` before intentionally shutting
down. A write failure pauses training and disables automatic scheduling; a
failed promotion save restores the previous active weights in RAM.

Model transfer commands are excluded from conversation-history logging to avoid
filling the Vault with hexadecimal weight chunks. Existing facts, lessons, and
conversation history retain their existing layout. Full-image reflashing resets
both the Vault and model store. v0.34 and earlier kernels ignore the new store;
returning to v0.35 can reload it.

## Desktop training and SSH model updates

The companion uses the system OpenSSH client in one session and enforces the
existing `known_hosts` entry. Log in normally first and verify the Pi's host key.
Passwords stay with OpenSSH's terminal/askpass handling. They are not arguments
or stored in the Python tool. `--known-hosts PATH` supports an explicit trusted
host-key file. Windows command examples require Python and NumPy as described in
`LANGUAGE-MODEL.md`. Open PowerShell in `source\Holly-AI-Learning-OS`:

```powershell
$env:OPENBLAS_NUM_THREADS = "1"
.\.venv\Scripts\python.exe model\sync.py --host 192.168.1.123 download holly-export
```

Replace the example IP. Use an empty destination directory. The tool pauses Holly,
waits for a checkpoint, then exports the active model, all approved dataset splits,
and a provenance manifest. These files may contain your private memories. Holly
remains paused afterward so the snapshot is consistent.

For the first desktop run from those Pi weights:

```powershell
.\.venv\Scripts\python.exe model\holly_lm.py train --train holly-export\train.txt --valid holly-export\valid.txt --warm-start holly-export\active.npz --steps 2000 --output model\candidate.npz
```

This starts Adam state for the exported weights. It trains all model layers.
For subsequent continuation from that desktop checkpoint:

```powershell
.\.venv\Scripts\python.exe model\holly_lm.py train --train holly-export\train.txt --valid holly-export\valid.txt --resume model\candidate.npz --steps 2000 --output model\candidate.npz
```

`--steps` means additional updates. Checkpoints contain the current weights,
Adam moments, update counter, random-generator state, and the best validation
checkpoint. The exporter/upload uses the best validation weights; resume uses
the latest current weights and optimizer. Atomic file replacement preserves the
last completed host checkpoint. A forced stop can lose up to 249 updates.

Resume refuses changed training/validation hashes unless you explicitly add
`--allow-data-change`. Retain old examples as replay data when doing so. With a
changed dataset, best-checkpoint selection restarts from the current weights;
optimizer state and random sequence continue. The old v0.34 `seed1.npz` lacks
optimizer state: use `--warm-start` for it, not `--resume`.

Upload the candidate without rebuilding the OS:

```powershell
.\.venv\Scripts\python.exe model\sync.py --host 192.168.1.123 upload model\candidate.npz
```

Uploading cancels any unfinished candidate and turns off automatic scheduling.
It sends the exact 79,972 model bytes with sequential offset checks and a whole
payload CRC over authenticated SSH. An incomplete upload never becomes active
and is discarded after reboot. A completed upload is checkpointed and evaluated
on the Pi while idle. Check `train status` later to see whether it was accepted.
Use `train auto on` again if you want automatic rounds to continue.

The companion is tested with Linux OpenSSH and the production SSH/training code.
Its Windows commands use portable subprocess/pipe handling but have not been
run on the user's Windows machine. No GPU acceleration is implemented.
