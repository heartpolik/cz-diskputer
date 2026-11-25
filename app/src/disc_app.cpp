#include "disc_app.h"

#include "logger.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dp_ui {
namespace {

namespace key = platform::key;
using cz::kHeight;
using cz::kStatusHeight;
using cz::kWidth;
using cz::make_box;
using cz::make_label;

constexpr int kDriveY = 19;
constexpr int kDriveH = 38;
constexpr int kListY = 60;
constexpr int kRowH = 15;
constexpr int kFooterY = 137;

const char* kHelp =
    "#63e2b7 DISCPUTER# - USB optical drive\n"
    "The computer sees a CD-ROM (or a disk)\n"
    "with the image you insert here.\n"
    "\n"
    "#63e2b7 CONNECT#\n"
    "Host/Slave switch (left side): Slave\n"
    "Cable: RIGHT USB-C to the computer\n"
    "(Ethernet and USB-A are off in Slave)\n"
    "\n"
    "#63e2b7 KEYS# (under the screen: 4..8)\n"
    "F / X   choose an image (no Fn needed)\n"
    "8 Enter insert it (swaps the disc)\n"
    "4 E     eject\n"
    "5 R     reconnect USB\n"
    "6 M     mode AUTO / CD / DISK\n"
    "7 S     stop the drive\n"
    "F5      rescan the folder   F6 theme\n"
    "F7 U    USB flash drive: copy images\n"
    "\n"
    "#63e2b7 MODES#\n"
    "CD    ISO 9660 / UDF images: installers\n"
    "DISK  read-only USB disk: .img, and\n"
    "      hybrid Linux ISOs if CD won't boot\n"
    "AUTO  CD for ISOs, DISK for the rest\n"
    "\n"
    "#63e2b7 COPY IMAGES IN#\n"
    "F7 (Fn 7) or U: the computer sees a\n"
    "flash drive DISCPUTER. Copy .iso files\n"
    "onto it, then EJECT it on the computer:\n"
    "they move to the list by themselves\n"
    "(8 = done, if it was just unplugged).\n"
    "\n"
    "#63e2b7 IMAGES#\n"
    "~/discputer/images (or scp over Wi-Fi)\n"
    "BOOT    bootable (El Torito)\n"
    "HYBRID  also boots as a USB disk\n"
    "BIN/CUE not yet (next version)\n"
    "\n"
    "The drive keeps working after you\n"
    "leave the app; S stops it.";

std::string color_hex(lv_color_t c) {
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%06x", static_cast<unsigned>(lv_color_to_u32(c) & 0xffffff));
    return buf;
}

std::filesystem::path default_images_dir() {
    if (const char* env = std::getenv("DP_IMAGES")) return env;
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / "discputer" / "images";
}

std::string base_name(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

DiscApp::DiscApp(cz::Runtime& rt)
    : rt_(rt), style_(cz::Style::make(rt.dark_mode(), rt.assets())), gadget_(make_gadget()),
      images_dir_(default_images_dir()) {
    std::error_code ec;
    std::filesystem::create_directories(images_dir_, ec);
    load();
    rescan();
    state_ = gadget_->state();
    build();
    rt_.set_layer_handler([this](cz::LayerState s) { bar_.show_layer(s); });
    // on a fresh install sudo has no rule for the helper yet: the answer opens the setup screen
    gadget_->start({"status"});
    timer_ = lv_timer_create([](lv_timer_t* t) { static_cast<DiscApp*>(lv_timer_get_user_data(t))->poll(); }, 300,
                             this);
}

DiscApp::~DiscApp() {
    rt_.set_layer_handler(nullptr);
    if (timer_) lv_timer_delete(timer_);
    save();
    destroy();
}

void DiscApp::rescan() {
    const std::string keep = sel_ < static_cast<int>(images_.size()) ? images_[static_cast<size_t>(sel_)].path : "";
    images_ = dp::scan(images_dir_, scan_error_);
    const int idx = dp::find_path(images_, keep);
    if (idx >= 0) sel_ = idx;
    sel_ = std::clamp(sel_, 0, std::max(0, static_cast<int>(images_.size()) - 1));
}

dp::Mode DiscApp::mode_for(const dp::Image& img) const {
    if (pref_ == ModePref::Cd) return dp::Mode::Cd;
    if (pref_ == ModePref::Disk) return dp::Mode::Disk;
    return img.info.recommended();
}

const dp::Image* DiscApp::inserted() const {
    const int idx = dp::find_path(images_, state_.file);
    return idx >= 0 ? &images_[static_cast<size_t>(idx)] : nullptr;
}

void DiscApp::build() {
    const auto& s = style_;
    root_ = make_box(lv_screen_active(), 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(root_, s.bg, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    auto* bar = make_box(root_, 0, 0, kWidth, kStatusHeight);
    lv_obj_set_style_bg_color(bar, s.surface, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    title_ = make_label(bar, s.ui10, s.accent, "DISCPUTER");
    lv_obj_set_pos(title_, 6, 2);
    usb_ = make_label(bar, s.ui10, s.dim);
    lv_label_set_recolor(usb_, true);
    lv_obj_align(usb_, LV_ALIGN_TOP_RIGHT, -6, 2);

    drive_ = make_box(root_, 4, kDriveY, kWidth - 8, kDriveH);
    lv_obj_set_style_bg_color(drive_, s.surface, 0);
    lv_obj_set_style_bg_opa(drive_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(drive_, 4, 0);
    lv_obj_set_style_border_width(drive_, 1, 0);
    drive_title_ = make_label(drive_, s.mono14, s.text);
    lv_obj_set_pos(drive_title_, 8, 3);
    lv_obj_set_width(drive_title_, kWidth - 24);
    lv_label_set_long_mode(drive_title_, LV_LABEL_LONG_MODE_DOTS);
    drive_info_ = make_label(drive_, s.ui10, s.dim);
    lv_obj_set_pos(drive_info_, 8, 22);
    lv_obj_set_width(drive_info_, kWidth - 24);
    lv_label_set_long_mode(drive_info_, LV_LABEL_LONG_MODE_DOTS);

    for (int i = 0; i < kRows; ++i) {
        rows_[i] = make_box(root_, 4, kListY + i * kRowH, kWidth - 8, kRowH);
        lv_obj_set_style_bg_color(rows_[i], s.selection, 0);
        lv_obj_set_style_radius(rows_[i], 2, 0);
        row_mark_[i] = make_label(rows_[i], s.mono12, s.accent);
        lv_obj_set_pos(row_mark_[i], 4, 0);
        row_name_[i] = make_label(rows_[i], s.mono12, s.text);
        lv_obj_set_pos(row_name_[i], 18, 0);
        lv_obj_set_width(row_name_[i], 220);
        lv_label_set_long_mode(row_name_[i], LV_LABEL_LONG_MODE_DOTS);
        row_size_[i] = make_label(rows_[i], s.mono12, s.dim);
        lv_obj_align(row_size_[i], LV_ALIGN_TOP_RIGHT, -4, 0);
    }
    footer_ = make_label(root_, s.ui10, s.dim);
    lv_obj_set_pos(footer_, 8, kFooterY);
    lv_obj_set_width(footer_, kWidth - 16);
    lv_label_set_long_mode(footer_, LV_LABEL_LONG_MODE_DOTS);

    bar_.build(root_, s);
    bar_.set(5, "rescan", true, cz::KeyLayer::Fn);
    bar_.set(6, "theme", true, cz::KeyLayer::Fn);
    bar_.set(7, "copy in", true, cz::KeyLayer::Fn);
    bar_.show_layer(rt_.key_layer());

    help_ = make_box(root_, 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(help_, s.bg, 0);
    lv_obj_set_style_bg_opa(help_, LV_OPA_COVER, 0);
    lv_obj_add_flag(help_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(help_, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_pad_all(help_, 6, 0);
    help_text_ = make_label(help_, s.mono12, s.text, kHelp);
    lv_label_set_recolor(help_text_, true);
    lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);

    setup_ = make_box(root_, 0, 0, kWidth, kHeight);
    lv_obj_set_style_bg_color(setup_, s.bg, 0);
    lv_obj_set_style_bg_opa(setup_, LV_OPA_COVER, 0);
    auto* st = make_label(setup_, s.ui12, s.accent, "FIRST RUN");
    lv_obj_set_pos(st, 8, 4);
    const char* user = std::getenv("USER");
    const std::string why = std::string(
                                "DiscPuter turns the USB port into a CD drive\n"
                                "or a flash drive: that needs root. Type the\n"
                                "password of user ") +
                            (user ? user : "pi") +
                            " once; it adds a sudo rule for\n"
                            "DiscPuter's own helper only:\n"
                            "/usr/lib/cz-discputer/dp-gadget";
    auto* sw = make_label(setup_, s.ui10, s.text, why.c_str());
    lv_obj_set_pos(sw, 8, 22);
    setup_field_ = make_box(setup_, 8, 98, kWidth - 16, 22);
    lv_obj_set_style_border_width(setup_field_, 1, 0);
    lv_obj_set_style_border_color(setup_field_, s.accent, 0);
    lv_obj_set_style_radius(setup_field_, 3, 0);
    auto* dots = make_label(setup_field_, s.mono14, s.text);
    lv_obj_set_pos(dots, 6, 2);
    setup_note_label_ = make_label(setup_, s.ui10, s.dim);
    lv_obj_set_pos(setup_note_label_, 8, 126);
    auto* keys = make_label(setup_, s.ui10, s.dim, "Enter  OK        Esc  later (USB stays off)");
    lv_obj_set_pos(keys, 8, 150);
    lv_obj_add_flag(setup_, LV_OBJ_FLAG_HIDDEN);
    refresh_setup();
    refresh();
}

void DiscApp::show_setup(bool on) {
    setup_on_ = on;
    if (!on) {
        password_.clear();
        setup_note_.clear();
    }
    refresh_setup();
}

void DiscApp::refresh_setup() {
    if (!setup_) return;
    if (!setup_on_) {
        lv_obj_add_flag(setup_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(setup_, LV_OBJ_FLAG_HIDDEN);
    std::string dots(password_.size(), '*');
    if (!setup_checking_) dots += "_";
    lv_label_set_text(lv_obj_get_child(setup_field_, 0), dots.c_str());
    const std::string note = setup_checking_ ? "checking..." : setup_note_;
    lv_label_set_text(setup_note_label_, note.c_str());
    lv_obj_set_style_text_color(setup_note_label_, setup_note_.empty() || setup_checking_ ? style_.dim : style_.error,
                                0);
}

bool DiscApp::setup_key(const cz::KeyEvent& ev) {
    if (!ev.pressed_or_repeat() || setup_checking_) return true;
    const uint32_t k = ev.key;
    if (k == key::Esc) {
        show_setup(false);
    } else if (k == key::Enter) {
        if (!password_.empty() && gadget_->start_setup(password_)) {
            setup_checking_ = true;
            password_.clear();
        }
    } else if (k == key::Backspace || k == key::Delete) {
        if (!password_.empty()) password_.pop_back();
    } else if (k >= 32 && k < 127 && password_.size() < 64) {
        password_.push_back(static_cast<char>(k));
        setup_note_.clear();
    }
    refresh_setup();
    return true;
}

void DiscApp::destroy() {
    if (root_) lv_obj_delete(root_);
    root_ = nullptr;
}

void DiscApp::poll() {
    CommandResult r;
    if (gadget_->take_result(r)) {
        busy_text_.clear();
        rescan();
        if (r.command == "setup") {
            setup_checking_ = false;
            if (r.ok) show_setup(false);
            else {
                setup_note_ = r.message;
                refresh_setup();
                r.message.clear();
            }
        } else if (r.needs_setup) {
            show_setup(true);
            r.message.clear();
        } else if (r.command == "status") {
            r.message.clear();  // the quiet start-up check
        }
        if (!r.message.empty()) {
            rt_.toast().show(r.message, r.ok ? view::widgets::ToastTone::Default : view::widgets::ToastTone::Error,
                             r.ok ? 1500 : 3500);
        }
    }
    const GadgetState now = gadget_->state();
    const bool changed = now.up != state_.up || now.file != state_.file || now.usb != state_.usb ||
                         now.cd != state_.cd || now.drop != state_.drop;
    state_ = now;
    // the computer ejected the flash drive: move what it copied into the images folder
    if (state_.up && state_.drop) {
        if (!state_.file.empty()) drop_in_use_ = true;
        else if (drop_in_use_ && !gadget_->busy()) {
            drop_in_use_ = false;
            command({"import"}, "Importing images");
        }
    }
    if (changed || r.ok || !r.message.empty()) refresh();
}

void DiscApp::refresh() {
    if (!root_) return;
    refresh_status();
    refresh_drive();
    refresh_list();
    refresh_footer();
    refresh_bar();
}

void DiscApp::refresh_bar() {
    const bool idle = !gadget_->busy();
    const bool up = state_.up;
    const bool have = !images_.empty() && images_[static_cast<size_t>(sel_)].info.supported();
    const char* mode = pref_ == ModePref::Auto ? "auto" : pref_ == ModePref::Cd ? "cd" : "disk";
    if (up && state_.drop) {
        bar_.set(4, "", false);
        bar_.set(5, "usb", idle);
        bar_.set(6, "", false);
        bar_.set(7, "", false);
        bar_.set(8, "done", idle);
        return;
    }
    bar_.set(4, "eject", idle && up && !state_.file.empty());
    bar_.set(5, "usb", idle && up);
    bar_.set(6, mode, idle);
    bar_.set(7, "stop", idle && up);
    bar_.set(8, up && !state_.file.empty() ? "swap" : "insert", idle && have);
}

void DiscApp::refresh_status() {
    const auto& s = style_;
    std::string text;
    const char* pref = pref_ == ModePref::Auto ? "AUTO" : pref_ == ModePref::Cd ? "CD" : "DISK";
    (void)pref;
    if (!state_.up) text += "#" + color_hex(s.dim) + " USB OFF#";
    else if (state_.drop) text += "#" + color_hex(s.warn) + " FLASH#";
    else if (state_.usb == "configured") text += "#" + color_hex(s.accent) + " ONLINE#";
    else text += "#" + color_hex(s.warn) + " NO HOST#";
    lv_label_set_text(usb_, text.c_str());
}

void DiscApp::refresh_drive() {
    const auto& s = style_;
    std::string title, info;
    lv_color_t border = s.border;
    if (!busy_text_.empty()) {
        title = busy_text_;
        info = "working...";
        border = s.warn;
    } else if (!state_.controller) {
        title = "No USB device controller";
        info = "dwc2 must be in OTG mode (config.txt)";
        border = s.error;
    } else if (!state_.up) {
        title = "Drive off";
        info = "Enter inserts the selected image";
    } else if (state_.drop) {
        title = "USB flash drive DISCPUTER";
        if (state_.usb != "configured") {
            border = s.warn;
            info = "no computer: Slave switch + right USB-C";
        } else {
            border = s.warn;
            info = "copy images on, then eject it there";
        }
    } else {
        const auto* img = inserted();
        const std::string kind = state_.cd ? "CD-ROM" : "USB disk";
        if (state_.file.empty()) {
            title = "No disc";
            info = kind + " is empty: Enter inserts one";
        } else {
            title = img && !img->info.label.empty() ? img->info.label : base_name(state_.file);
            info = kind + "  " + (img ? dp::format_size(img->info.size) : "") + "  " + base_name(state_.file);
        }
        if (state_.usb == "configured") {
            border = s.accent;
        } else {
            border = s.warn;
            info = "no computer: Slave switch + right USB-C";
        }
    }
    lv_obj_set_style_border_color(drive_, border, 0);
    lv_label_set_text(drive_title_, title.c_str());
    lv_label_set_text(drive_info_, info.c_str());
}

void DiscApp::refresh_list() {
    const auto& s = style_;
    const int n = static_cast<int>(images_.size());
    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + kRows) first_ = sel_ - kRows + 1;
    first_ = std::clamp(first_, 0, std::max(0, n - kRows));
    for (int i = 0; i < kRows; ++i) {
        const int idx = first_ + i;
        if (idx >= n) {
            lv_obj_set_style_bg_opa(rows_[i], LV_OPA_TRANSP, 0);
            lv_label_set_text(row_mark_[i], "");
            lv_label_set_text(row_size_[i], "");
            if (i == 3 && n == 0 && scan_error_.empty()) {
                lv_obj_set_style_text_color(row_name_[i], s.dim, 0);
                lv_label_set_text(row_name_[i], "or U: copy them in over USB");
            } else if (i == 1 && n == 0) {
                lv_obj_set_style_text_color(row_name_[i], s.dim, 0);
                lv_label_set_text(row_name_[i], scan_error_.empty() ? "no images: copy .iso files to" : scan_error_.c_str());
            } else if (i == 2 && n == 0 && scan_error_.empty()) {
                lv_obj_set_style_text_color(row_name_[i], s.dim, 0);
                lv_label_set_text(row_name_[i], images_dir_.string().c_str());
            } else {
                lv_label_set_text(row_name_[i], "");
            }
            continue;
        }
        const auto& img = images_[static_cast<size_t>(idx)];
        const bool is_in = state_.up && img.path == state_.file;
        lv_obj_set_style_bg_opa(rows_[i], idx == sel_ ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_label_set_text(row_mark_[i], is_in ? "*" : "");
        lv_obj_set_style_text_color(row_name_[i], img.info.supported() ? (is_in ? s.accent : s.text) : s.dim, 0);
        lv_label_set_text(row_name_[i], img.name.c_str());
        lv_label_set_text(row_size_[i], dp::format_size(img.info.size).c_str());
        lv_obj_align(row_size_[i], LV_ALIGN_TOP_RIGHT, -4, 0);
    }
}

void DiscApp::refresh_footer() {
    if (images_.empty()) {
        lv_label_set_text(footer_, "F1 help");
        return;
    }
    const auto& img = images_[static_cast<size_t>(sel_)];
    std::string text;
    if (!img.info.label.empty()) text += img.info.label + "  ";
    text += img.info.tags();
    if (img.info.supported()) text += std::string("  -> ") + dp::mode_name(mode_for(img));
    else text += "  (next version)";
    lv_label_set_text(footer_, text.c_str());
}

void DiscApp::command(const std::vector<std::string>& args, const std::string& busy_text) {
    if (gadget_->busy()) return;
    if (gadget_->start(args)) {
        busy_text_ = busy_text;
        refresh();
    }
}

void DiscApp::insert_selected() {
    if (images_.empty()) return;
    const auto& img = images_[static_cast<size_t>(sel_)];
    if (!img.info.supported()) {
        rt_.toast().show("BIN/CUE comes in the next version", view::widgets::ToastTone::Default, 2000);
        return;
    }
    const dp::Mode mode = mode_for(img);
    const bool same_mode = state_.cd == (mode == dp::Mode::Cd);
    if (state_.up && same_mode) {
        // swap + reconnect: every host notices the new disc that way
        command({"swap", img.path, "replug"}, "Inserting " + img.name);
    } else {
        command({"up", img.path, mode == dp::Mode::Cd ? "cd" : "disk"}, "Inserting " + img.name);
    }
}

void DiscApp::copy_in() {
    if (state_.up && state_.drop) return;
    command({"drop"}, "Preparing the flash drive");
}

bool DiscApp::action(int key) {
    if (state_.up && state_.drop) {
        if (key == 5) command({"replug"}, "Reconnecting USB");
        if (key == 8) {
            drop_in_use_ = false;
            command({"import"}, "Importing images");
        }
        return key >= 4 && key <= 8;
    }
    switch (key) {
        case 4:
            if (state_.up && !state_.file.empty()) command({"eject"}, "Ejecting");
            return true;
        case 5:
            if (state_.up) command({"replug"}, "Reconnecting USB");
            return true;
        case 6: {
            pref_ = pref_ == ModePref::Auto ? ModePref::Cd : pref_ == ModePref::Cd ? ModePref::Disk : ModePref::Auto;
            // a disc in the drive follows the new mode right away
            const auto* img = inserted();
            if (state_.up && img && state_.cd != (mode_for(*img) == dp::Mode::Cd)) {
                command({"up", img->path, mode_for(*img) == dp::Mode::Cd ? "cd" : "disk"}, "Switching mode");
            }
            return true;
        }
        case 7:
            if (state_.up) command({"down"}, "Stopping the drive");
            return true;
        case 8:
            insert_selected();
            return true;
        default:
            return false;
    }
}

void DiscApp::toggle_help() {
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(help_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_scroll_to_y(help_, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(help_, LV_OBJ_FLAG_HIDDEN);
}

bool DiscApp::on_key(const cz::KeyEvent& ev) {
    if (setup_on_) return setup_key(ev);
    const bool press = ev.phase == cz::KeyPhase::Press;
    const uint32_t k = ev.key;
    if (!lv_obj_has_flag(help_, LV_OBJ_FLAG_HIDDEN)) {
        if (!ev.pressed_or_repeat()) return true;
        if (k == key::Up || k == 'f') lv_obj_scroll_by_bounded(help_, 0, 36, LV_ANIM_OFF);
        else if (k == key::Down || k == 'x') lv_obj_scroll_by_bounded(help_, 0, -36, LV_ANIM_OFF);
        else if (press && (k == key::Esc || k == key::Help || k == key::F(1) || k == 'q')) toggle_help();
        return true;
    }
    if (!ev.pressed_or_repeat()) return false;
    const int n = static_cast<int>(images_.size());

    if (press && cz::ShortcutBar::is_key(k)) {
        action(cz::ShortcutBar::key_of(k));
        refresh();
        return true;
    }
    switch (k) {
        case key::Up:
        case 'f':  // F/X/Z/C are arrows on CardputerZero without Fn (launcher convention)
            if (n) sel_ = (sel_ + n - 1) % n;
            break;
        case key::Down:
        case 'x':
            if (n) sel_ = (sel_ + 1) % n;
            break;
        default:
            if (!press) return false;
            switch (k) {
                case key::Help:
                case key::F(1):
                    toggle_help();
                    return true;
                case key::Enter:
                    action(8);
                    break;
                case 'e':
                case 'E':
                    action(4);
                    break;
                case 'r':
                case 'R':
                    action(5);
                    break;
                case 'm':
                case 'M':
                    action(6);
                    break;
                case 's':
                case 'S':
                    action(7);
                    break;
                case 'u':
                case 'U':
                case key::F(7):
                    copy_in();
                    break;
                case key::F(5):
                    rescan();
                    rt_.toast().show(std::to_string(images_.size()) + " images", view::widgets::ToastTone::Default,
                                     1200);
                    break;
                case key::F(6):
                    save();
                    rt_.set_dark_mode(!rt_.dark_mode());
                    style_ = cz::Style::make(rt_.dark_mode(), rt_.assets());
                    destroy();
                    build();
                    return true;
                default:
                    return false;
            }
            break;
    }
    refresh();
    return true;
}

void DiscApp::load() {
    std::ifstream in(rt_.state_dir() / "state.txt");
    std::string line;
    std::string selected;
    while (std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string k = line.substr(0, tab), v = line.substr(tab + 1);
        if (k == "mode") pref_ = v == "cd" ? ModePref::Cd : v == "disk" ? ModePref::Disk : ModePref::Auto;
        if (k == "selected") selected = v;
    }
    images_ = dp::scan(images_dir_, scan_error_);
    const int idx = dp::find_path(images_, selected);
    if (idx >= 0) sel_ = idx;
}

void DiscApp::save() {
    std::ofstream out(rt_.state_dir() / "state.txt", std::ios::trunc);
    if (!out) return;
    out << "mode\t" << (pref_ == ModePref::Cd ? "cd" : pref_ == ModePref::Disk ? "disk" : "auto") << '\n';
    if (sel_ < static_cast<int>(images_.size())) out << "selected\t" << images_[static_cast<size_t>(sel_)].path << '\n';
}

}  // namespace dp_ui
