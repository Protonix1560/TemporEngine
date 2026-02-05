

#ifndef HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_
#define HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_


#include "core.hpp"



class HardwareMemoryOptimizator {

    public:
        void init();
        void update();
        void shutdown() noexcept;


    private:
        ServiceLocator mLoc;

};


#endif  // HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_
