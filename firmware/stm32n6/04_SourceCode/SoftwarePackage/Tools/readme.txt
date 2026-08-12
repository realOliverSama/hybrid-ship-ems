每个新例程的使用方法

假设当前工程是：
SoftwarePackage/Projects/03_Button2

在 VS Code 打开该工程根目录，CubeMX 生成 CMake 后，终端执行：
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --dry-run

这是预览模式，只显示将要修改什么，不实际写入。

确认无误后执行：
python3 ../../Tools/atk_stm32n6_cmake_prepare.py

然后在 VS Code 中：
执行 CMake: Delete Cache and Reconfigure；
执行 CMake: Build。

使用 Release 生成的.bin 与.hex 文件保存于：
....../Appli/build/Release/flash

以后每次用 CubeMX 重新生成代码，只需重新运行：
python3 ../../Tools/atk_stm32n6_cmake_prepare.py