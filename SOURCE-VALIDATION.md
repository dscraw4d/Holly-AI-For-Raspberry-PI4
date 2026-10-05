# Source release verification - 5 October 2026

The sanitised source folder compiled from scratch using AArch64 GNU GCC, with a newly provisioned test identity (not included in the source archive). The resulting raw kernel was 7,496,240 bytes. ELF/raw-image matching, no unresolved symbols, memory bounds and the live Pi networking/credential gate passed.

The documented copy-to-bootfiles and image-builder workflow completed, creating a 256 MiB image and validating its MBR, FAT32 and boot-file readback. This test image had its own test host identity and is not a replacement for the already distributed Lite binary.

`make access-test document-test assistant-test web-ui-test` passed from this source folder. Logs are in release-evidence/source-*.log. This establishes a buildable source package and these software checks, not physical Pi acceptance. The existing Lite release's physical first-boot test remains pending.

PDF: 17 rendered pages, all inspected as a contact sheet; detailed development/upload pages reviewed separately. Text extraction and text bounding boxes checked; matching manuals share one authored chapter source.
