#!/usr/bin/env python3
"""
将编译好的程序写入 Flash 镜像文件
"""

import os
import sys

def update_flash_image(flash_img='flash.img', program_bin='smartos.bin'):
    """
    将程序二进制文件写入 Flash 镜像的开头
    """
    if not os.path.exists(flash_img):
        print(f"Error: Flash image '{flash_img}' not found!")
        print("Run create_flash.py first to create the Flash image.")
        sys.exit(1)
    
    if not os.path.exists(program_bin):
        print(f"Error: Program binary '{program_bin}' not found!")
        print("Run 'make' first to compile the program.")
        sys.exit(1)
    
    # 读取程序二进制
    with open(program_bin, 'rb') as f:
        program_data = f.read()
    
    program_size = len(program_data)
    print(f"Program size: {program_size} bytes ({program_size / 1024:.2f} KB)")
    
    # 读取 Flash 镜像
    with open(flash_img, 'rb') as f:
        flash_data = bytearray(f.read())
    
    flash_size = len(flash_data)
    print(f"Flash size: {flash_size} bytes ({flash_size / 1024:.2f} KB)")
    
    # 检查程序是否太大
    if program_size > flash_size:
        print(f"Error: Program ({program_size} bytes) is larger than Flash ({flash_size} bytes)!")
        sys.exit(1)
    
    # 将程序写入 Flash 镜像的开头
    flash_data[0:program_size] = program_data
    
    # 写回 Flash 镜像
    with open(flash_img, 'wb') as f:
        f.write(flash_data)
    
    print(f"Updated Flash image: {flash_img}")
    print(f"Program written to address 0x00000000")
    print(f"File system area starts at 0x00010000 (64 KB)")

if __name__ == '__main__':
    update_flash_image()
