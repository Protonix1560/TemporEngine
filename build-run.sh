#!/bin/bash

if [ -z ${COMPILE_ENGINE+x} ]; then
    COMPILE_ENGINE=1
fi
if [ -z ${COMPILE_TEST_PLUGIN+x} ]; then
    COMPILE_TEST_PLUGIN=1
fi

echo -e "\033[92m    //// BUILD ////\033[0m\n"
./build.sh && {
    printf '\n%.0s' $(seq 1 $(tput lines))
    tput cup 0 0
    echo -e "\033[92m    //// TEST ////\033[0m\n"
    cd test
    ./launch.sh || exit 1
    cd ..
    echo
}