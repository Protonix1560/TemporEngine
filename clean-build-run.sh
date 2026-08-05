#!/bin/bash

if [ -z ${COMPILE_ENGINE+x} ]; then
    COMPILE_ENGINE=1
fi
if [ -z ${COMPILE_TEST_PLUGIN+x} ]; then
    COMPILE_TEST_PLUGIN=1
fi

echo -e "\033[92m    //// CLEANUP ////\033[0m\n"

echo removing build directories
rm -rf build/tempor
rm -rf build/plugins

echo removing test directory
rm -rf test

echo

./build-run.sh