# 运行 QEMU 模拟器，使用 pflash 设备

# 首先创建 Flash 镜像（如果不存在）
if (-not (Test-Path "flash.img")) {
    Write-Host "Creating Flash image..."
    python create_flash.py
}

# 将编译好的程序写入 Flash 镜像的开头
Write-Host "Updating Flash image with smartos.bin..."
python update_flash.py

# 启动 QEMU，使用 pflash 设备
Write-Host "Starting QEMU with pflash device..."
Write-Host "Press Ctrl+A then X to exit QEMU"
Write-Host ""

qemu-system-arm -M lm3s6965evb -nographic -drive if=pflash,format=raw,file=flash.img -serial mon:stdio

# 注意：
# -drive if=pflash,format=raw,file=flash.img  将 flash.img 映射到 0x00000000
# Flash 是可读写的，修改会保存到 flash.img 文件中
