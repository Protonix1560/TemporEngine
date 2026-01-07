#!/bin/bash

if [ -z ${COMPILE_ENGINE+x} ]; then
    COMPILE_ENGINE=1
fi
if [ -z ${COMPILE_TEST_PLUGIN+x} ]; then
    COMPILE_TEST_PLUGIN=1
fi

echo -e "\033[92m    //// BUILD ////\033[0m\n"
./build.sh &&
{
    if [[ COMPILE_ENGINE -eq 1 ]]; then
        make install -j${nproc} -C build/tempor || exit 1
    fi
    if [[ COMPILE_TEST_PLUGIN -eq 1 ]]; then
        make install -j${nproc} -C build/plugins/test || exit 1
    fi

} && {
    printf '\n%.0s' $(seq 1 $(tput lines))
    tput cup 0 0
    echo -e "\033[92m    //// TEST ////\033[0m\n"
    cd test
    ./launch.sh || exit 1
    cd ..
    echo
}