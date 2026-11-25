# DiscPuter — контекст

Відповідай українською. Власник — Sebastian. Емулятор USB CD/DVD-приводу на M5Stack CardputerZero (CM0, Debian trixie),
ідея з USBODE (danifunker/usbode-circle, GPL-3.0) → цей репо теж GPL-3.0. Основна мета — монтувати образи для встановлення ОС.

## Як влаштовано
- Збирається через сусідній `../cardputer-zero` (cardputer-zero-tools, MIT): `cmake -S ../cardputer-zero -DCZ_APP_DIR=$PWD/app`
  (параметри `CZ_APP_DIR`, `CZ_DIST_DIR`, `LICENSE` у `cz_package_app` додано саме для цього). `scripts/dp sim|test|shots|deb|push|run`.
- `app/core/image_info` (ISO 9660 PVD/мітка, El Torito, UDF NSR02/03, MBR/GPT → hybrid; рекомендований режим CD/DISK), `library` (скан теки).
- `app/src/gadget` — стан читається з configfs без root; зміни — `sudo -n /usr/libexec/discputer/dp-gadget up|swap|eject|replug|down`
  у фоновому потоці. На десктопі — `SimGadget`. `disc_app` — один екран.
- `app/packaging/dp-gadget` = `scripts/cz-gadget.sh` + перевірка шляху (лише `~/discputer/images`, `/media`, `/mnt`, читабельний для SUDO_USER).
  postinst пише `/etc/sudoers.d/discputer` (`%plugdev`, після `visudo -cf`), prerm робить `down`, postrm прибирає правило.

## Залізо (перевірено 2026-11-25)
- Перемикач Host/Slave зліва: Slave → USB CM0 на ПРАВИЙ USB-C; хаб (Ethernet, USB-A, лівий USB-C, EXT USB) вимкнений. Wi-Fi лишається (SSH живе).
- UDC `3f980000.usb` (dwc2, dr_mode otg), модулі `usb_f_mass_storage`, `usb_f_fs` є. Гаджет у configfs: `usb_gadget/discputer`.
- macOS: CD монтується; заміна диска — `swap` і `replug` обидва працюють (застосунок робить swap+replug).

## Ітерації
0 — тест скриптом (CD+swap ✅ на Маку; завантаження з інсталятора на PC — ще треба). A — застосунок 0.1.0 (ISO/IMG через ядро) ✅ на пристрої.
B — свій гаджет через FunctionFS з SCSI/MMC: BIN/CUE, CD-audio (ES8389), сумісність зі старими ОС.
