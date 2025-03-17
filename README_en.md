# `qtools`

A set of tools for working with flash modems on Qualcomm chipsets

The set consists of a utility package and a set of patched bootloaders.

## Loaders

The programs require modified loaders in order to work, which are in the
`loaders/` directory.

The patch source is in `loaders/pexec_arm.asm` / `loaders/pexec_thumb.asm`.

## Tools

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

## Recovery process

### Initialization

start with
```sh
qdload -i -t -k3 loaders/ENPRG9x25p.bin
```

- `-k3` for the chipset, in this case `MDM9x25`; `-kl` to get a list
- `-i` for normal init
- `-t` to save the partition table from MIBIB

unclear what is "patched" in the loaders

### Partitions

on success, list partitions with
```sh
qrflash -k3 -m
```

and save all of them with
```sh
qrflash -k3 -f '*'
```

### Open issues

- unclear whether writing to NAND is stable
- replacing ABOOT (Android Bootloader) with lk2nd did not work
- trying to revert back to the original boot loader did not boot
