#include "gadget.h"

#include <atomic>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace dp_ui {
namespace fs = std::filesystem;

namespace {

constexpr const char* kGadgetDir = "/sys/kernel/config/usb_gadget/discputer";
constexpr const char* kLun = "/sys/kernel/config/usb_gadget/discputer/functions/mass_storage.usb0/lun.0";
constexpr const char* kHelper = "/usr/lib/cz-discputer/dp-gadget";

std::string read_line(const std::string& path) {
    std::ifstream in(path);
    std::string s;
    std::getline(in, s);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

std::string last_line(const std::string& text) {
    std::istringstream in(text);
    std::string line, last;
    while (std::getline(in, line)) {
        if (!line.empty() && line.rfind("sudo: unable to resolve host", 0) != 0) last = line;
    }
    if (last.rfind("error: ", 0) == 0) last = last.substr(7);
    return last;
}

// runs a command with input on its stdin, returns its exit code and combined output
int run_capture(const std::vector<std::string>& argv, const std::string& input, std::string& output) {
    int pipefd[2];
    int infd[2];
    if (pipe(pipefd) != 0) {
        output = std::string("pipe: ") + std::strerror(errno);
        return -1;
    }
    if (pipe(infd) != 0) {
        output = std::string("pipe: ") + std::strerror(errno);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        output = std::string("fork: ") + std::strerror(errno);
        close(pipefd[0]);
        close(pipefd[1]);
        close(infd[0]);
        close(infd[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(infd[0], STDIN_FILENO);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(infd[0]);
        close(infd[1]);
        close(pipefd[0]);
        close(pipefd[1]);
        std::vector<char*> args;
        for (const auto& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(pipefd[1]);
    close(infd[0]);
    // small (a password line): fits the pipe buffer, so no deadlock against the reader below
    if (!input.empty()) {
        ssize_t w = write(infd[1], input.data(), input.size());
        (void)w;
    }
    close(infd[1]);
    char buf[512];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof buf)) > 0) output.append(buf, static_cast<size_t>(n));
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

class BackgroundCommands : public Gadget {
public:
    ~BackgroundCommands() override {
        if (worker_.joinable()) worker_.join();
    }
    bool start(const std::vector<std::string>& args) override { return launch(args, ""); }
    bool start_setup(const std::string& password) override { return launch({"setup"}, password); }
    bool launch(const std::vector<std::string>& args, const std::string& password) {
        if (busy_) return false;
        if (worker_.joinable()) worker_.join();
        busy_ = true;
        worker_ = std::thread([this, args, password] {
            CommandResult r = execute(args, password);
            r.command = args.empty() ? "" : args[0];
            std::lock_guard<std::mutex> lock(mu_);
            result_ = r;
            has_result_ = true;
            busy_ = false;
        });
        return true;
    }
    bool busy() const override { return busy_; }
    bool take_result(CommandResult& out) override {
        std::lock_guard<std::mutex> lock(mu_);
        if (!has_result_) return false;
        out = result_;
        has_result_ = false;
        return true;
    }

protected:
    // password: only for setup (sudo -S); empty = sudo -n
    virtual CommandResult execute(const std::vector<std::string>& args, const std::string& password) = 0;

private:
    std::thread worker_;
    std::atomic<bool> busy_{false};
    std::mutex mu_;
    CommandResult result_;
    bool has_result_{false};
};

// ------------------------------------------------------------ the device

class LinuxGadget : public BackgroundCommands {
public:
    GadgetState state() override {
        GadgetState s;
        std::error_code ec;
        s.up = fs::exists(kGadgetDir, ec);
        if (s.up) {
            s.file = read_line(std::string(kLun) + "/file");
            s.cd = read_line(std::string(kLun) + "/cdrom") != "0";
            s.drop = !s.cd && read_line(std::string(kLun) + "/ro") == "0";
        }
        s.controller = false;
        for (const auto& u : fs::directory_iterator("/sys/class/udc", ec)) {
            s.controller = true;
            s.usb = read_line((u.path() / "state").string());
            break;
        }
        return s;
    }

protected:
    CommandResult execute(const std::vector<std::string>& args, const std::string& password) override {
        CommandResult r;
        if (!fs::exists(kHelper)) {
            r.message = "helper missing: reinstall the package";
            return r;
        }
        std::vector<std::string> argv = {"sudo"};
        if (password.empty()) argv.push_back("-n");
        else argv.insert(argv.end(), {"-S", "-k", "-p", ""});  // password from stdin, no prompt text
        argv.push_back(kHelper);
        argv.insert(argv.end(), args.begin(), args.end());
        std::string out;
        const int code = run_capture(argv, password.empty() ? "" : password + "\n", out);
        r.ok = code == 0;
        r.message = last_line(out);
        if (!r.ok && out.find("a password is required") != std::string::npos) {
            r.needs_setup = true;
            r.message = "first run: DiscPuter needs your password once";
        } else if (!r.ok && !password.empty() &&
                   (out.find("incorrect password") != std::string::npos ||
                    out.find("Sorry, try again") != std::string::npos)) {
            r.message = "wrong password";
        } else if (!r.ok && !password.empty() && out.find("not in the sudoers") != std::string::npos) {
            r.message = "this user may not use sudo";
        }
        return r;
    }
};

// ------------------------------------------------------------ the desktop simulator

class SimGadget : public BackgroundCommands {
public:
    GadgetState state() override {
        std::lock_guard<std::mutex> lock(mu_);
        return s_;
    }

protected:
    CommandResult execute(const std::vector<std::string>& args, const std::string& password) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::lock_guard<std::mutex> lock(mu_);
        CommandResult r{true, ""};
        const std::string cmd = args.empty() ? "" : args[0];
        // DP_SIM_SETUP=1: start like a fresh install, any password but "wrong" works
        if (cmd == "setup") {
            r.ok = password != "wrong";
            r.message = r.ok ? "ready: DiscPuter can drive USB now" : "wrong password";
            if (r.ok) needs_setup_ = false;
            return r;
        }
        if (needs_setup_) {
            r.ok = false;
            r.needs_setup = true;
            r.message = "first run: DiscPuter needs your password once";
            return r;
        }
        if (cmd == "status") {
            r.message = "gadget: down";
        } else if (cmd == "up") {
            s_.up = true;
            s_.file = args.size() > 1 ? args[1] : "";
            s_.cd = args.size() < 3 || args[2] != "disk";
            s_.drop = false;
            s_.usb = "configured";
            r.message = "host: configured";
        } else if (cmd == "drop") {
            s_.up = true;
            s_.cd = false;
            s_.drop = true;
            s_.file = "/var/lib/cz-discputer/drop.img";
            s_.usb = "configured";
            r.message = "USB flash drive DISCPUTER";
        } else if (cmd == "import") {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            s_ = GadgetState{};
            s_.usb = "not attached";
            r.message = "imported 0 files";
        } else if (cmd == "swap") {
            s_.file = args.size() > 1 ? args[1] : "";
            r.message = "inserted";
        } else if (cmd == "eject") {
            s_.file.clear();
            r.message = "ejected";
        } else if (cmd == "replug") {
            r.message = "reconnected";
        } else if (cmd == "down") {
            s_ = GadgetState{};
            s_.usb = "not attached";
            r.message = "gadget: down";
        }
        return r;
    }

private:
    std::mutex mu_;
    bool needs_setup_{std::getenv("DP_SIM_SETUP") != nullptr};
    GadgetState s_{false, true, false, "", "not attached", true};
};

}  // namespace

std::unique_ptr<Gadget> make_gadget() {
#if USE_DESKTOP
    return std::make_unique<SimGadget>();
#else
    return std::make_unique<LinuxGadget>();
#endif
}

}  // namespace dp_ui
