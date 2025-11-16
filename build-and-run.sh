#!/bin/bash

cd build

echo -e "\033[92m    //// BUILD ////\033[0m\n"

cmake --fresh -DCMAKE_BUILD_TYPE=Debug .. && \
make -j${nproc} && \
{

    printf '\n%.0s' $(seq 1 $(tput lines))
    tput cup 0 0

    echo -e "\033[92m    //// TEST ////\033[0m\n"

    cp tempor ../test/tempor
    cp test_plugin/libtest_plugin.so ../test/plugins/libtest_plugin.so
    cd ../test
    export LSAN_OPTIONS=suppressions=../leaks.txt
    ./launch-test.sh
    echo
}