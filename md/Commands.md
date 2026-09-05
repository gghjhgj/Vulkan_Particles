rm -rf build
cmake --preset release
cmake --build --preset build-release
cmake --build build/release --target shader
build/release/bin/app.exe