#!/usr/bin/env bash

echo "Formating c/c++ files..."
shopt -s globstar # to activate the ** globbing
clang_format_exe='clang-format'
if ! command -v ${clang_format_exe} &> /dev/null; then
    clang_format_exe='clang-format-11'
fi
${clang_format_exe} --version
${clang_format_exe} -i $(find lct -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
${clang_format_exe} -i $(find src -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
${clang_format_exe} -i $(find examples -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")
${clang_format_exe} -i $(find tests -type f -name "*.h" -o -name "*.c" -o -name "*.hpp" -o -name "*.cpp")

echo "Formating cmake files..."
cmake-format --version
cmake-format -i **/*.cmake **/CMakeLists.txt

echo "Done."
