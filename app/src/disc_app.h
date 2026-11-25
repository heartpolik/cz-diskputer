#pragma once

#include "gadget.h"
#include "library.h"
#include "runtime.h"
#include "shortcut_bar.h"
#include "ui_style.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace dp_ui {

// One screen: the drive (what the computer sees) and the images folder.
class DiscApp {
public:
    static constexpr int kRows = 5;

    explicit DiscApp(cz::Runtime& rt);
    ~DiscApp();

    bool on_key(const cz::KeyEvent& ev);

private:
    enum class ModePref { Auto, Cd, Disk };

    void build();
    void destroy();
    void rescan();
    void poll();
    void refresh();
    void refresh_status();
    void refresh_drive();
    void refresh_list();
    void refresh_footer();
    void refresh_bar();
    bool action(int key);
    void toggle_help();
    void insert_selected();
    void command(const std::vector<std::string>& args, const std::string& busy_text);
    dp::Mode mode_for(const dp::Image& img) const;
    const dp::Image* inserted() const;
    void load();
    void save();

    cz::Runtime& rt_;
    cz::Style style_;
    std::unique_ptr<Gadget> gadget_;
    std::filesystem::path images_dir_;
    std::vector<dp::Image> images_;
    std::string scan_error_;
    GadgetState state_;
    int sel_{0};
    int first_{0};
    ModePref pref_{ModePref::Auto};
    std::string busy_text_;
    lv_timer_t* timer_{nullptr};

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* usb_{nullptr};
    lv_obj_t* drive_{nullptr};
    lv_obj_t* drive_title_{nullptr};
    lv_obj_t* drive_info_{nullptr};
    lv_obj_t* rows_[kRows]{};
    lv_obj_t* row_mark_[kRows]{};
    lv_obj_t* row_name_[kRows]{};
    lv_obj_t* row_size_[kRows]{};
    lv_obj_t* footer_{nullptr};
    cz::ShortcutBar bar_;
    lv_obj_t* help_{nullptr};
    lv_obj_t* help_text_{nullptr};
};

}  // namespace dp_ui
