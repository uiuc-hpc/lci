echo "Formating source..."
find . -name "*.cxx" | xargs clang-format -i

echo "Formating header..."
find . -name "*.h" | xargs clang-format -i

echo "Done."
