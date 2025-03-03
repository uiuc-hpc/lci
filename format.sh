#!/usr/bin/env bash

echo "Formating c/c++ files..."
shopt -s globstar # to activate the ** globbing
if ! command -v clang-format &> /dev/null; then
    alias clang-format='clang-format-11'
fi
clang-format --version
clang-format -i $(find lct -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format -i $(find src -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format -i $(find examples -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format -i $(find tests -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")

echo "Formating cmake files..."
cmake-format --version
cmake-format -i **/*.cmake **/CMakeLists.txt

echo "Done."
