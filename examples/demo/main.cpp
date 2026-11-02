#include "demo_window.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    guikit::demo::DemoWindow app;
    return app.Run();
}
