# RISC-V Emulator and Linux System from Scratch

This repo contains all the source code required to follow the blog post at https://werwolv.net/posts/linux_bringup/

## Source Code
- `/lib` contains the entire rv32ima instruction set emulator code as well as a minimal set of emulated devices
- `/main` contains the configuration for our machine and functions exposed for WebAssembly use

## Configuration
All the config files required to build Linux can be found in `/boot`. The main executable also expects all the built binaries to be in the `boot` folder.

- `riscv32-unknown-linux-gnu.config` is the crosstool-ng toolchain config file
- `riscv_emulator_defconfig` is the Linux Kernel config file
- `device_tree.dts` is the minimal device tree
  - Can be compiled to a dtb using `dtc -I dts -O dtb device_tree.dts -o device_tree.dtb`
- `init.c` is the init program
  - Can be compiled using `riscv32-unknown-linux-gnu-gcc -static init.c -o init`
- `initramfs.txt` is the `gen_init_cpio` config file to generate the initramfs
  - Can be compiled to a cpio using `usr/gen_init_cpio initramfs.txt -o initramfs.cpio`
