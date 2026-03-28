# mamtool Bug Tracker / Audit Checklist

## Critical

- [x] **Heap buffer overflow in `attribute_set_value`**
  `GC_MALLOC(ma->length)` allocates too little when format is ASCII/TEXT — `memset` writes `vbuflen` (length+1) bytes, overflowing by 1. Fixed: allocate `vbuflen` instead.

- [x] **CDB allocation length truncated to 8 bits**
  READ ATTRIBUTE / WRITE ATTRIBUTE CDB allocation/parameter length is a 4-byte big-endian field (bytes 10-13 per SPC-4), but only byte 13 (LSB) was written. Fixed: write all 4 bytes big-endian.

## High

- [x] **Unaligned memory access / strict aliasing UB**
  `ma->value` cast directly to `uint16_t*`, `uint32_t*`, `uint64_t*`. Fixed: `memcpy` into a local typed variable.

- [x] **`attribute_to_buffer` doesn't match SPC-4 WRITE ATTRIBUTE format**
  Hardcoded offsets, parameter data length only wrote LSB, attribute length MSB never set, no bounds checking, `strncpy` didn't null-terminate. Fixed: rewritten to match SPC-4 spec with constants and bounds check.

- [x] **Off-by-one in `attribute_id_to_string`**
  Loop condition `i <= ATTR_DEF_NUM` read one past the array. Fixed: use `< ARRAY_SIZE(attr_def)`.

## Medium

- [x] **Dead error check in `mam_read_attribute_1`**
  Return value of `attribute_set_value` (can be ENOMEM) was never captured. Fixed: `error = attribute_set_value(...)`.

- [x] **`uci_print_pretty` buffer overread**
  `rawval += 4` after reading 2-byte `cartridge_type`, `manufacturer` not null-terminated. Fixed.

- [x] **`ucialt_print_pretty` same overread bug**
  Same `rawval += 4` issue after 2-byte field, `serial` not null-terminated. Fixed.

- [x] **REQUEST SENSE uses wrong direction flag** (`uscsi_subr.c`)
  REQUEST SENSE reads data from device but used `SCSI_WRITECMD`. Fixed: changed to `SCSI_READCMD`.

- [x] **Sense data `sense_key` field was actually SKS value** (`uscsi_subr.c`, `uscsilib.h`)
  Bytes 16-17 are Sense Key Specific, not the sense key (byte 2 bits 3:0). Fixed: extract real sense key from byte 2, renamed SKS fields.

- [x] **`RDATTR_HEADONLY_LEN` missing parentheses**
  Macro expanded unsafely in compound expressions. Fixed: wrapped in parentheses.

- [x] **`assert()` used for runtime error handling**
  Memory allocation failures, user input validation, and SCSI errors used `assert()`. Fixed: replaced with `fprintf(stderr, ...)` + `exit(EXIT_FAILURE)` or return codes.

## Low

- [x] **Typo: `lenght` in `struct mam_attribute_definition`**
  Fixed: renamed to `length`.

- [x] **`ATTR_DEF_NUM` manually maintained**
  Fixed: replaced with `ARRAY_SIZE` macro.

- [x] **Non-exclusive operation flags**
  `-L`, `-r`, `-w`, `-u` could all be passed together. Fixed: enforced exactly one operation.

- [x] **Boehm GC dependency**
  Replaced with standard `malloc`/`strdup`/`free`. Removed `-lgc` from Makefiles.

- [ ] **`attribute_id_to_string` returns pointer to static buffer**
  Two calls for unknown IDs overwrite each other. Latent bug, not triggered currently.

- [ ] **Global `struct uscsi_dev dev`**
  Non-reentrant global state. Would need refactor to pass as parameter.

- [ ] **`endian_utils.c` reimplements standard functions**
  Keeping custom implementation for portability.

- [ ] **No `S_ISCHR` check on device path** (`uscsi_subr.c`)
  `fstat` doesn't verify the path is a character device.

- [x] **Missing partition number support in CDB**
  MAM is per-cartridge; partition 0 is correct for single-partition tapes. Won't fix — current behavior matches use case.

- [ ] **No Makefile for FreeBSD** despite `USCSI_FREEBSD_CAM` support in code.
