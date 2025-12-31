#!/usr/bin/env python3
"""创建磁盘镜像文件"""
import os
import sys

DISK_SIZE = 512 * 1024  # 512KB = 1024 sectors

def create_disk_image(filename="disk.img"):
    """创建指定大小的磁盘镜像文件"""
    if os.path.exists(filename):
        print(f"{filename} already exists, skipping creation")
        return
    
    with open(filename, 'wb') as f:
        f.write(b'\x00' * DISK_SIZE)
    
    print(f"Created {filename} ({DISK_SIZE} bytes)")

if __name__ == "__main__":
    create_disk_image()

