# DiscPuter — контекст

Відповідай українською. Власник — Sebastian. Емулятор USB CD/DVD-приводу на M5Stack CardputerZero (CM0, Debian trixie),
ідея з USBODE (danifunker/usbode-circle, GPL-3.0) → цей репо теж GPL-3.0. Основна мета — монтувати образи для встановлення ОС.

## Як влаштовано
- Збирається через сусідній `../cardputer-zero` (cardputer-zero-tools, MIT): `cmake -S ../cardputer-zero -DCZ_APP_DIR=$PWD/app`
  (параметри `CZ_APP_DIR`, `CZ_DIST_DIR`, `LICENSE` у `cz_package_app` додано саме для цього). `scripts/dp sim|test|shots|deb|push|run`.
- `app/core/image_info` (ISO 9660 PVD/мітка, El Torito, UDF NSR02/03, MBR/GPT → hybrid; рекомендований режим CD/DISK), `library` (скан теки).
- `app/src/gadget` — стан читається з configfs без root; зміни — `sudo -n /usr/lib/cz-discputer/dp-gadget up|swap|eject|replug|down|drop|import`
  у фоновому потоці. На десктопі — `SimGadget`. `disc_app` — один екран.
- `app/packaging/dp-gadget` = `scripts/cz-gadget.sh` + перевірка шляху (лише `~/discputer/images`, `/media`, `/mnt`, читабельний для SUDO_USER).
  **Стор** (CardputerZero/packages) відхиляє maintainer-скрипти зі згадкою `/etc/sudoers`, `mkfs`, `rm /var…` (grep у validate-pr.yml)
  і шляхи поза `usr/lib/<pkg>/`, `usr/share/<pkg>/`, `var/lib/<pkg>/`… (install_path_policy.py). Тому: postinst/postrm немає;
  правило `/etc/sudoers.d/cz-discputer` пише сам хелпер (`dp-gadget setup`) — застосунок при першому запуску (sudo -n → «password is
  required») показує екран FIRST RUN, бере пароль користувача й робить `sudo -S -k -p "" … setup`. prerm: `remove` → `dp-gadget uninstall`
  (down + umount + прибирає правило), інакше `down`.
- Флешка-приймач (0.2.0): `dp-gadget drop` — sparse exFAT `/var/lib/cz-discputer/drop.img` (вільне − 2 ГіБ, ≤ 64 ГіБ, свіжий щоразу),
  LUN rw; Eject на хості → ядро чистить `lun.0/file` → застосунок сам робить `import` (mount loop у `/run/cz-discputer/drop`, файли
  копіюються в `~/discputer/images` від імені користувача через `runuser`, крапкові/службові macOS пропускаються, drop.img видаляється).
  Заміряно: ~10 МБ/с на запис з Мака і ~10 МБ/с імпорт.

## Залізо (перевірено 2026-11-25)
- Перемикач Host/Slave зліва: Slave → USB CM0 на ПРАВИЙ USB-C; хаб (Ethernet, USB-A, лівий USB-C, EXT USB) вимкнений. Wi-Fi лишається (SSH живе).
- UDC `3f980000.usb` (dwc2, dr_mode otg), модулі `usb_f_mass_storage`, `usb_f_fs` є. Гаджет у configfs: `usb_gadget/discputer`.
- macOS: CD монтується; заміна диска — `swap` і `replug` обидва працюють (застосунок робить swap+replug).

## Публікація
- Репо: github.com/heartpolik/cz-diskputer (публічний). Метадані стору: `app/app-builder.json` (≤ 6 скриншотів, summary ≤ 80).
- Перевірити deb локально тими ж скриптами, що й CI стору: `.github/scripts/install_path_policy.py` + grep небезпечних шаблонів.

## Ітерації
0 — тест скриптом (CD+swap ✅ на Маку; завантаження з інсталятора на PC — ще треба). A — застосунок 0.1.0 (ISO/IMG через ядро) ✅ на пристрої.
B — свій гаджет через FunctionFS з SCSI/MMC: BIN/CUE, CD-audio (ES8389), сумісність зі старими ОС.
