# STM32N647 project rules

## Directory layout

- Shared preparation script: ../../Tools/atk_stm32n6_cmake_prepare.py
- Shared BSP root: ../../Drivers/BSP
- Do not copy the preparation script or BSP into this project.
- Preserve the overall SoftwarePackage directory structure.

## Generated code

- Prefer modifying CubeMX USER CODE regions.
- Do not modify the .ioc file unless explicitly authorized.
- Do not overwrite generated files wholesale.
- Put CMake customizations in user-maintained regions where available.

## Verification

- After implementation, build the Debug configuration.
- Continue fixing implementation-related build errors until Debug succeeds.
- Report pre-existing or unrelated errors separately.

## Hardware safety

Without explicit authorization, do not:

- connect to ST-LINK;
- program or erase Flash;
- start a GDB server;
- modify option bytes;
- reset or control the board;
- run release-stm32n647-appli;
- perform Git push or create a pull request.
