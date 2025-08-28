

#include "tempor.hpp"


#ifdef LSAN_BUILT_IN_OPTIONS
extern "C" const char* __lsan_default_options() {
    return "suppressions=../leaks.txt";
}
#endif


int main(int argc, char* argv[]) {
    TemporEngine engine;
    return engine.run(argc, argv);
}
