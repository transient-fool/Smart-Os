@echo off
REM 运行 QEMU 模拟器，使用 pflash 设备

REM 首先创建 Flash 镜像（如果不存在）
if not exist flash.img (
    echo Creating Flash image...
    python create_flash.py
)

REM 将编译好的程序写入 Flash 镜像的开头
echo Updating Flash image with smartos.bin...
python update_flash.py

REM 启动 QEMU，使用 pflash 设备
echo Starting QEMU with pflash device...
qemu-system-arm -M lm3s6965evb -nographic -drive if=pflash,format=raw,file=flash.img -serial mon:stdio

REM 注意：
REM -drive if=pflash,format=raw,file=flash.img  将 flash.img 映射到 0x00000000
REM Flash 是可读写的，修改会保存到 flash.img 文件中
