# Dedup
A command line application that finds duplicate files and removes them. Duplicate files can also be replaced with symbolic links or hard links.

The application can be built using CMake. Example:
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```