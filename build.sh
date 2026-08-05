#!/bin/bash

if [ -z ${COMPILE_ENGINE+x} ]; then
    COMPILE_ENGINE=1
fi
if [ -z ${COMPILE_TEST_PLUGIN+x} ]; then
    COMPILE_TEST_PLUGIN=1
fi

./config-build.sh &&
{
    if [[ COMPILE_ENGINE -eq 1 ]]; then
        make install -j$(( $(nproc) - 1 )) -C build/tempor || exit 1
    fi
    if [[ COMPILE_TEST_PLUGIN -eq 1 ]]; then
        make install -j$(( $(nproc) - 1 )) -C build/plugins/test || exit 1
    fi
}