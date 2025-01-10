#!/usr/bin/env bash

echo "Formating c/c++ files..."
shopt -s globstar # to activate the ** globbing
clang-format-11 --version
clang-format-11 -i $(find lct -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format-11 -i $(find src -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format-11 -i $(find examples -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
clang-format-11 -i $(find tests -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")

echo "Formating cmake files..."
cmake-format --version
cmake-format -i **/*.cmake **/CMakeLists.txt

echo "Done."
