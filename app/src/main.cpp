#include "disc_app.h"
#include "runtime.h"

#ifndef DP_DEV_ASSETS
#define DP_DEV_ASSETS ""
#endif

int main() {
    cz::Runtime rt("cz-discputer", USE_DESKTOP ? DP_DEV_ASSETS : "");
    if (!rt.init()) return 1;

    int code = 0;
    {
        dp_ui::DiscApp app(rt);
        rt.set_key_handler([&app](const cz::KeyEvent& ev) { return app.on_key(ev); });
        code = rt.run();
        rt.set_key_handler(nullptr);
    }
    return code;
}
