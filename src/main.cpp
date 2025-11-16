

#if !defined(__linux__)
    #error "Unsupported OS type"
#endif



#include "tempor.hpp"


int main(int argc, char* argv[]) {
    TemporEngine engine;
    return engine.run(argc, argv);
}
