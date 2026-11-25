#!/usr/bin/env bash
# Iteration 0: show a disc image to a computer as a USB CD-ROM (or a read-only disk)
# through the Linux mass-storage gadget. Run on the CardputerZero as root.
#
#   sudo cz-gadget.sh up <image> [cd|disk]   create the gadget and "insert" the image
#   sudo cz-gadget.sh swap <image> [replug]  change the disc (replug: also reconnect USB,
#                                            for hosts that miss the media change, e.g. macOS)
#   sudo cz-gadget.sh replug                 disconnect and reconnect USB (host re-reads the drive)
#   sudo cz-gadget.sh eject                  take the disc out (drive stays)
#   sudo cz-gadget.sh status                 what the host sees
#   sudo cz-gadget.sh down                   remove the gadget
#
# The Host/Slave switch on the left side must be on Slave: then the CM0 USB goes to
# the RIGHT USB-C port (the USB hub, Ethernet and USB-A are off while it is there).
#
# cd   : SCSI CD-ROM (ISO 9660 / El Torito boot), what installers on CD expect.
# disk : read-only removable disk; better for "hybrid" Linux ISOs and some UEFI firmware.
set -euo pipefail

G=/sys/kernel/config/usb_gadget/discputer
F=$G/functions/mass_storage.usb0
LUN=$F/lun.0

die() { echo "error: $*" >&2; exit 1; }
need_root() { [[ $EUID -eq 0 ]] || die "run as root (sudo)"; }

udc_name() {
    local u
    u=$(ls /sys/class/udc 2>/dev/null | head -1)
    [[ -n $u ]] || die "no USB device controller (dwc2 must be in otg/peripheral mode)"
    echo "$u"
}

check_image() {
    local img=$1
    [[ -f $img ]] || die "no such image: $img"
    readlink -f "$img"
}

up() {
    need_root
    local img mode=${2:-cd}
    img=$(check_image "${1:-}")
    [[ $mode == cd || $mode == disk ]] || die "mode is cd or disk"
    [[ -d $G ]] && die "gadget already exists: use swap, or down first"

    modprobe libcomposite
    modprobe usb_f_mass_storage
    mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config

    mkdir -p "$G"
    echo 0x1d6b > "$G/idVendor"    # Linux Foundation
    echo 0x0104 > "$G/idProduct"   # Multifunction Composite Gadget
    echo 0x0100 > "$G/bcdDevice"
    echo 0x0200 > "$G/bcdUSB"
    mkdir -p "$G/strings/0x409"
    echo "CardputerZero" > "$G/strings/0x409/manufacturer"
    echo "DiscPuter" > "$G/strings/0x409/product"
    echo "DP$(cut -c9-16 /proc/device-tree/serial-number 2>/dev/null || echo 00000001)" > "$G/strings/0x409/serialnumber"

    mkdir -p "$G/configs/c.1/strings/0x409"
    echo "DiscPuter" > "$G/configs/c.1/strings/0x409/configuration"
    echo 250 > "$G/configs/c.1/MaxPower"

    mkdir -p "$F"
    echo 1 > "$F/stall"
    echo 1 > "$LUN/removable"
    echo 1 > "$LUN/ro"
    if [[ $mode == cd ]]; then
        echo 1 > "$LUN/cdrom"
        # vendor(8) product(16) revision(4)
        printf '%-8s%-16s%-4s' "CZ" "DiscPuter CD" "0001" > "$LUN/inquiry_string"
    else
        echo 0 > "$LUN/cdrom"
        printf '%-8s%-16s%-4s' "CZ" "DiscPuter Disk" "0001" > "$LUN/inquiry_string"
    fi
    echo "$img" > "$LUN/file"
    ln -s "$F" "$G/configs/c.1/"

    udc_name > "$G/UDC"
    echo "up: $(basename "$img") as $mode on $(cat "$G/UDC")"
    wait_configured
}

swap() {
    need_root
    local img
    img=$(check_image "${1:-}")
    [[ -d $G ]] || die "gadget is not up"
    # the host may hold the medium locked; forced_eject ignores that
    echo 1 > "$LUN/forced_eject" 2>/dev/null || echo "" > "$LUN/file"
    sleep 1
    echo "$img" > "$LUN/file"
    echo "inserted: $(basename "$img")"
    [[ ${2:-} == replug ]] && replug
    return 0
}

# soft unplug + plug: the host drops the drive and enumerates it again with the current disc
replug() {
    need_root
    [[ -d $G ]] || die "gadget is not up"
    local udc
    udc=$(cat "$G/UDC")
    [[ -n $udc ]] || udc=$(udc_name)
    echo "" > "$G/UDC"
    sleep 1
    echo "$udc" > "$G/UDC"
    echo "reconnected"
    wait_configured
}

# the host needs a moment to enumerate after (re)connecting
wait_configured() {
    local u state
    for _ in $(seq 1 20); do
        for u in /sys/class/udc/*; do
            state=$(cat "$u/state" 2>/dev/null || true)
            [[ $state == configured ]] && { echo "host: configured"; return 0; }
        done
        sleep 0.25
    done
    echo "host: not configured yet (${state:-?}); check the switch (Slave) and the right USB-C"
}

eject() {
    need_root
    [[ -d $G ]] || die "gadget is not up"
    echo 1 > "$LUN/forced_eject" 2>/dev/null || echo "" > "$LUN/file"
    echo "ejected"
}

status() {
    if [[ ! -d $G ]]; then
        echo "gadget: down"
    else
        local udc file
        udc=$(cat "$G/UDC" 2>/dev/null || true)
        file=$(cat "$LUN/file" 2>/dev/null || true)
        echo "gadget: up (udc ${udc:-none}, mode $([[ $(cat "$LUN/cdrom") == 1 ]] && echo cd || echo disk))"
        echo "disc:   ${file:-(empty)}"
    fi
    for u in /sys/class/udc/*; do
        [[ -e $u ]] || continue
        echo "usb:    $(basename "$u") state=$(cat "$u/state")"
    done
    echo "  (configured = the host enumerated us; not attached = cable/switch: Slave + right USB-C?)"
}

down() {
    need_root
    [[ -d $G ]] || { echo "gadget: down"; return; }
    echo "" > "$G/UDC" 2>/dev/null || true
    rm -f "$G/configs/c.1/mass_storage.usb0"
    rmdir "$G/configs/c.1/strings/0x409" "$G/configs/c.1" 2>/dev/null || true
    rmdir "$LUN" 2>/dev/null || true
    rmdir "$F" "$G/strings/0x409" "$G" 2>/dev/null || true
    echo "gadget: down"
}

cmd=${1:-status}
shift || true
case $cmd in
    up) up "$@" ;;
    swap) swap "$@" ;;
    replug) replug ;;
    eject) eject ;;
    status) status ;;
    down) down ;;
    *) sed -n '2,15p' "$0"; exit 1 ;;
esac
