
echo -e "\033[92m    //// TEST ////\033[0m\n"
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=dist -DBUILD_TESTING=ON \
    -B build/tempor -S . -G "Unix Makefiles" || exit 1 &&
cmake --build build/tempor --target mat_tests &&
ctest --test-dir build/tempor -R '^mat_tests$' --output-on-failure
