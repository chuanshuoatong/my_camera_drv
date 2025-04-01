# 本内核模块是基于Amlogic V918D_2.2 SDK的，在编译该模块前，要先完成SDK的编译，在out/android12-5.4/common下生成内核头
# 关于GCC版本，实际测试了7.5/11.2/13.2/14.0都可以成功编译
# 关于CLANG，这里使用的是SDK中预置的，要保证与SDK中编译内核时使用的CLANG版本一致
# 关于在make时须同时指定GCC交叉编译工具链和CLANG相关工具链，缺一不可
# 

# 指定内核源码路径
KERN_DIR = /home/rentong/nvme-data/rentong/V918D_2.2/out/android12-5.4/common


# GCC工具链位置
GCC_7_5_DIR = /home/rentong/kernel/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin
GCC_11_2_DIR = /home/rentong/kernel/gcc-linaro-11.2.1-2021.10-x86_64_aarch64-linux-gnu/bin
GCC_13_0_DIR = /home/rentong/kernel/gcc-linaro-13.0.0-2022.06-x86_64_aarch64-linux-gnu/bin
GCC_14_0_DIR = /home/rentong/kernel/gcc-linaro-14.0.0-2023.06-x86_64_aarch64-linux-gnu/bin


# CLANG工具链位置
CLANG_DIR= /nvme-data/rentong/V918D_2.2/prebuilts/clang/host/linux-x86/clang-r383902/bin


# 指定目标架构
ARCH = arm64


# 交叉编译工具前缀
CROSS_COMPILE = $(GCC_14_0_DIR)/aarch64-linux-gnu-


CC=$(CLANG_DIR)/clang
LD=$(CLANG_DIR)/ld.lld
NM=$(CLANG_DIR)/llvm-nm


# 编译选项：添加额外的头文件路径
EXTRA_CFLAGS += -I$(KERN_DIR)/include \
                -I$(KERN_DIR)/arch/$(ARCH)/include \
                -I$(KERN_DIR)/arch/$(ARCH)/include/generated \
                -I$(KERN_DIR)/include/uapi


# 编译目标
obj-m += my_camera.o

all:
	make -C $(KERN_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CC=$(CC) LD=$(LD) NM=$(NM) M=$(PWD) modules

clean:
	make -C $(KERN_DIR) M=$(PWD) clean