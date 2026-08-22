rm -rf build
cmake --preset release
cmake --build --preset build-release
cmake --build build/release --target shader
build/release/bin/app.exe

| Nazwa      |   Rozdzielczość |      Piksele |
| ---------- | --------------: | -----------: |
| 360p       |       640 × 360 |      0,23 MP |
| 480p       |       854 × 480 |      0,41 MP |
| 540p       |       960 × 540 |      0,52 MP |
| 720p       |      1280 × 720 |      0,92 MP |
| 900p       |      1600 × 900 |      1,44 MP |
| 1080p      |     1920 × 1080 |      2,07 MP |
| 1440p      |     2560 × 1440 |      3,69 MP |
| 1620p      |     2880 × 1620 |      4,67 MP |
| 1800p      |     3200 × 1800 |      5,76 MP |
| 2160p / 4K |     3840 × 2160 |      8,29 MP |
| 2880p / 5K |     5120 × 2880 |     14,75 MP |
| 3240p      |     5760 × 3240 |     18,66 MP |
| **6K**     | **6144 × 3456** | **21,23 MP** |
