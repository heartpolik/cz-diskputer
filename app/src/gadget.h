// The USB side: what the kernel gadget shows the computer, and changing it through
// the root helper (sudo dp-gadget ...). On the desktop a simulated drive stands in.
#pragma once

#include "image_info.h"

#include <memory>
#include <string>
#include <vector>

namespace dp_ui {

struct GadgetState {
    bool up{false};          // the drive exists (configfs gadget)
    bool cd{true};           // CD-ROM, else read-only disk
    std::string file;        // the inserted image, empty = no disc
    std::string usb;         // "configured" (a computer uses us), "not attached", ...
    bool controller{true};   // a USB device controller exists at all
};

struct CommandResult {
    bool ok{false};
    std::string message;  // last line of the helper's output
};

class Gadget {
public:
    virtual ~Gadget() = default;
    virtual GadgetState state() = 0;
    // starts a helper command in the background; false while another one runs
    virtual bool start(const std::vector<std::string>& args) = 0;
    virtual bool busy() const = 0;
    // the finished command's result, once
    virtual bool take_result(CommandResult& out) = 0;
};

// the real one on the device, the simulated one on the desktop
std::unique_ptr<Gadget> make_gadget();

}  // namespace dp_ui
