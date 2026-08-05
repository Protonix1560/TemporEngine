#!/bin/bash

if [ -z ${COMPILE_ENGINE+x} ]; then
    COMPILE_ENGINE=1
fi
if [ -z ${COMPILE_TEST_PLUGIN+x} ]; then
    COMPILE_TEST_PLUGIN=1
fi

# tempor build
if [[ COMPILE_ENGINE -eq 1 ]]; then
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=test \
        -B build/tempor -S . -G "Unix Makefiles" || exit 1
fi

# test plugin build
if [[ COMPILE_TEST_PLUGIN -eq 1 ]]; then
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=test \
        -B build/plugins/test -S plugins/test/ -G "Unix Makefiles" || exit 1
fi

# generating final compile_commands.json
{
    SEARCH_DIR="."
    OUTPUT="build/compile_commands.json"
    FILES=$(find "$SEARCH_DIR" -name compile_commands.json)
    echo "[]" > "$OUTPUT"
    for f in $FILES; do
        jq -s '.[0] + .[1]' "$OUTPUT" "$f" > "$OUTPUT.tmp" && mv "$OUTPUT.tmp" "$OUTPUT"
    done
    echo Generated compile_commands.json
}