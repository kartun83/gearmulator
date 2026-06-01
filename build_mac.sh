cmake -G Xcode -S . -B ./temp/cmake_macos
cmake --build ./temp/cmake_macos --config Release
cd ./temp/cmake_macos
CMAKE_BIN="$(dirname "$(command -v cmake)")"
"${CMAKE_BIN}/cpack" -G ZIP
