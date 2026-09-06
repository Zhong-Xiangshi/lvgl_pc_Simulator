# LVGL PC Simulator

LVGL 9.x PC 模拟器工程（MinGW + SDL2 + FreeType），主程序入口 `main.c`。

## 编译命令

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" --fresh ..   # -G 指定 MinGW 编译器，--fresh 重新配置 build 目录
cmake --build . -j12                    # -j12 表示 12 线程编译
```

产物为 `build/simulator.exe`。

## 本机构建工具路径

`cmake`/`mingw32-make` 不在默认 PATH 中，实际位置：

- CMake: `F:/cmake-4.4.3-windows-x86_64/bin/cmake.exe`
- Make: `D:/mingw64/bin/mingw32-make.exe`（gcc 等工具链在 `D:/mingw64/bin`，已在 PATH）
- 编译命令可写为 `cmake --build . -j12`，或直接 `/d/mingw64/bin/mingw32-make.exe -j12`