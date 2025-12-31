#!/usr/bin/env python3
"""
创建 QEMU 使用的 Flash 镜像文件
LM3S6965 有 256KB Flash，我们创建一个完整的镜像
"""

import sys

def create_flash_image(output_file, size_kb=256):
    """
    创建一个指定大小的 Flash 镜像文件
    初始化为 0xFF（未编程的 Flash 状态）
    """
    size_bytes = size_kb * 1024
    
    # 创建全 0xFF 的数据
    data = bytearray([0xFF] * size_bytes)
    
    # 写入文件
    with open(output_file, 'wb') as f:
        f.write(data)
    
    print(f"Created Flash image: {output_file}")
    print(f"Size: {size_kb} KB ({size_bytes} bytes)")
    print(f"Initial state: 0xFF (unprogrammed)")

if __name__ == '__main__':
    output = 'flash.img'
    if len(sys.argv) > 1:
        output = sys.argv[1]
    
    create_flash_image(output, 256)
