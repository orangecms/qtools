A set of tools for working with flash modems on Qualcomm chipsets

The set consists of a utility package and a set of patched bootloaders.

The programs require modified loaders in order to work, which are in the
`loaders/` directory. The patch source is in `cmd_05_write_patched.asm`.

TODO: Where is this `cmd_05_write_patched.asm`?

### `qcommand`

An interactive terminal for entering commands via the command port.
It replaces the terribly inconvenient revskills.
Enter byte-by-byte command packets, edit memory, read and view any flash sector.

### `qrmem`

Read a dump of the modem address space.

### `qrflash`

Read flash. Can read both a range of blocks and partitions on a partition map.

### `qwflash`

Write partition images via the user partitions mode of the bootloader, similar
to QPST.

### `qwdirect`

Directly write flash drive blocks with / without OOB via the controller ports
(without the involvement of the bootloader logic).

### `qdload`

Load code when the modem is in download mode or emergency boot mode PBL.

### `dload.sh`

Script for switching the modem to boot mode and sending a loader to it.
