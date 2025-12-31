CROSS_COMPILE = arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

CFLAGS = -mcpu=cortex-m3 -mthumb -O0 -g -Wall -Icore -Idrivers -DQEMU_ENV -DENABLE_SHELL=1
LDFLAGS = -mcpu=cortex-m3 -mthumb -T link.lds -Wl,--gc-sections -nostartfiles

SRCS := user/main.c \
        user/snake_game.c \
        core/smart_core.c \
        core/smart_mempool.c \
        core/smart_fs.c \
        core/smart_shell.c \
        core/smart_msgqueue.c \
        core/smart_sync.c \
        core/smart_banner.c \
        core/smart_timer.c \
        drivers/smart_uart.c \
        drivers/smart_block.c

ASM_SRCS := startup.S \
            arch/context.S

OBJS := $(SRCS:.c=.o) $(ASM_SRCS:.S=.o)

TARGET = smartos

QEMU = qemu-system-arm
QEMU_MACHINE = lm3s6965evb
# 可选：使用versatilepb（支持PL181 SD卡控制器）
# QEMU_MACHINE = versatilepb

all: $(TARGET).elf $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

qemu: $(TARGET).elf
	$(QEMU) -M $(QEMU_MACHINE) -kernel $< -nographic -serial mon:stdio

# 创建磁盘镜像（如果不存在）
disk.img:
	python create_disk.py || python3 create_disk.py

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-del /Q user\main.o user\snake_game.o core\smart_core.o core\smart_mempool.o core\smart_fs.o core\smart_shell.o core\smart_msgqueue.o core\smart_sync.o core\smart_banner.o core\smart_timer.o drivers\smart_uart.o drivers\smart_block.o startup.o arch\context.o smartos.elf smartos.bin
