#include "core/App.h"
#include <iostream>

int main() {
    try {
        HuanGL::App app;
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << "\n";
        return -1;
    }
    return 0;
}
