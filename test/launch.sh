#!/bin/bash

cd ../build

echo -e "\033[92m    //// BUILD ////\033[0m\n"

cmake .. && \
make -j${nproc} && \
{

    printf '\n%.0s' $(seq 1 $(tput lines))
    tput cup 0 0

    echo -e "\033[92m    //// TEST ////\033[0m\n"

    cd ../test
    ../build/tempor --verbose=6
    echo
}