

#ifndef HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_
#define HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_



#include "core.hpp"



class HardwareMemoryOptimizator {

    public:
        HardwareMemoryOptimizator();
        void update();
        ~HardwareMemoryOptimizator() noexcept;


    private:

};

REGISTER_TYPE_NAME_S(HardwareMemoryOptimizator, "HWMO");


#endif  // HARDWARE_MEMORY_OPTIMIZATOR_HWMO_HPP_
