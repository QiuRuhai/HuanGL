#include "core/App.h"
#include "pipeline/Bvh.h"
#include <cstdio>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    // Early-exit selftest path: run CPU BVH self-tests without touching OpenGL.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--selftest") {
            bool ok = HuanGL::BvhSelfTest::RunAll();
            std::printf("selftest: %s\n", ok ? "PASS" : "FAIL");
            return ok ? 0 : 1;
        }
    }

    try {
        HuanGL::App app;
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return -1;
    }
    return 0;
}
