# DiscPuter

A USB optical drive emulator for the M5Stack CardputerZero: keep OS installer images on the SD card,
plug the CardputerZero into a computer and it sees a CD/DVD drive with the disc you picked on the
CardputerZero's own screen and keyboard.

Inspired by [USBODE](https://github.com/danifunker/usbode-circle) (danifunker, GPL-3.0), which does
this bare-metal on a Raspberry Pi Zero with Circle. DiscPuter runs on the CardputerZero's Linux
(Raspberry Pi CM0) and is a separate project; parts of its later CD emulation may be ported from
USBODE, so it is GPL-3.0 as well.

## Hardware

- **Host/Slave switch** (left side) on **Slave**: the CM0 USB goes straight to the **right USB-C**
  port. The internal USB hub is off then: no Ethernet, USB-A, left USB-C or EXT USB. Wi-Fi works.
- USB device controller: `dwc2` (`/sys/class/udc/3f980000.usb`, device tree `dr_mode = otg`).
- Kernel functions available: `usb_f_mass_storage` (CD-ROM or disk), `usb_f_fs` (gadget in userspace).

## Iterations

| # | What | Status |
|---|---|---|
| 0 | `scripts/cz-gadget.sh`: kernel mass-storage gadget by hand, does a computer see a CD and boot from it? | testing |
| A | CardputerZero app: list images, insert / eject / swap, CD or disk mode, through the kernel gadget (ISO only) | planned |
| B | own gadget over FunctionFS with SCSI/MMC: BIN/CUE, CD audio, multi-disc, better old-OS compatibility | planned |

## Iteration 0: test

On the CardputerZero (as root):

```bash
sudo ~/discputer/cz-gadget.sh up ~/discputer/images/dptest1.iso      # CD-ROM with disc 1
sudo ~/discputer/cz-gadget.sh status                                  # state=configured: the host sees us
sudo ~/discputer/cz-gadget.sh swap ~/discputer/images/dptest2.iso     # change the disc
sudo ~/discputer/cz-gadget.sh eject
sudo ~/discputer/cz-gadget.sh down
```

`up <image> disk` shows the image as a read-only removable disk instead of a CD: hybrid Linux ISOs
and some UEFI firmware prefer that.

Checklist:

1. Data ISO on a Mac / PC: a CD drive appears and the disc mounts (`dptest1.iso` → `HELLO.TXT`).
2. Swap: the computer notices the new disc (`dptest2.iso` → `SWAPPED.TXT`) without replugging.
3. Boot: a real installer ISO (Linux, Windows) boots on a PC from the "USB CD" in BIOS/UEFI.
4. Big images: a > 4 GB DVD ISO (Windows 10/11) reads fine.
5. `disk` mode with a hybrid Linux ISO boots where `cd` mode does not (or the other way round).

## License

GPL-3.0, see [LICENSE](LICENSE).
