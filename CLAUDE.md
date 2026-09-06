# LVGL PC Simulator

LVGL 9.x PC 模拟器工程（MinGW + SDL2 + FreeType），主程序入口 `main.c`。

## 编译命令

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" --fresh ..   # -G 指定 MinGW 编译器，--fresh 重新配置 build 目录
cmake --build . -j12                    # -j12 表示 12 线程编译
```

产物为 `build/simulator.exe`。