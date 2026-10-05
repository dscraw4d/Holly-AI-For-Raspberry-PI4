# Holly AI Learning OS v0.49.29

A fresh Raspberry Pi 4 fan-computer release with Holly, Hilly and Queeg, persistent knowledge uploads and browser speech. This documentation/source package describes the existing v0.49.29 kernel; no new runtime behaviour is claimed.

## Downloads

- Lite Pi 4 ZIP: complete 256 MiB raw image for a fresh card; 512 MB or larger practical minimum. Expands marked storage to the card capacity.
- Source ZIP: kernel, drivers, models/compiled weights, authored data, portrait inputs, embedded web source, vendor dependencies, tools, tests and manuals. No generated private credentials or populated Vault.
- PDF and TXT manuals: first boot, chat/voice, uploads, scripts/books, memories, notes/alarms, online lookup, maintenance and building.

## Current highlights

Queeg now selects an identified English US male browser voice. Holly/Hilly retain UK voices. Includes all three portraits, stern Queeg replies, retryable numbered HDMI mouth events, persistent 256-topic summaries, arithmetic, NEWS, notes/alarms and background document reading.

## Fresh setup

Write the extracted .img with Raspberry Pi Imager's Use custom option, connect wired Ethernet, find its DHCP address and log in as Rimmer / smeghead. On the new empty card run storage, doc format and doc status. Open http://PI_IP/. Do not format a populated document bank.

## Validation and limits

Software image structure/readback and simulated capacity expansion passed, along with existing v0.49.29 kernel/network/browser checks. Fresh Lite physical first-boot acceptance is pending. Browser microphone needs a supported secure origin and permission; plain LAN HTTP does not supply that. Numeric Host/HTTP Origin restrictions need compatible reverse-proxy headers. Small original models/retrieval are not ChatGPT-level AI. No complete TV script/novel collection is included.

## Maintainer publishing checklist

Suggested Git tag: v0.49.29. Upload Lite image ZIP, source ZIP, TXT/PDF manuals and SHA256SUMS as GitHub release assets. Copy this file into the release body. Upload the source ZIP's folder contents into the repository, not the outer ZIP as a replacement for code. Confirm first-party licence choice and media permissions before making open-source/media redistribution claims. Complete a real fresh-card Pi 4 acceptance test and update validation records before labelling the release hardware-stable.
