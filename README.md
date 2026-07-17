# Cpp-OS

A minimal x86 (i386, protected mode) operating system written in C++, NASM, and CMake.
Cold, dark, cyan-on-black terminal aesthetic. Hardware text cursor is synced to the
actual VGA cursor registers, so it tracks wherever you're typing.

## Build

Requires: nasm, cmake, g++ (with -m32 support), qemu-system-i386

```
mkdir build && cd build
cmake ..
make
```

This produces `build/cppos.img`, a raw floppy disk image containing the bootloader and kernel.

## Run

```
cd build
make run
```

or directly:

```
qemu-system-i386 -fda build/cppos.img
```
this may be unapdated
## Shell commands

- `help`     - list available commands
- `poke <hex addr> <hex byte>`  - writes a byte to physical memory (complement to peek); tested by writing directly into VGA memory and watching the character appear live on screen.
- `beep`  -  programs PIT channel 2 and toggles the PC speaker gate for a 1000 Hz tone; confirmed it runs and returns control to the shell without hanging.
- `rand` - xorshift PRNG seeded from the uptime tick counter.
- `reverse <text>` - in-place string reversal (hello → olleh, verified).
- `about`    - information about Cpp-OS
- `echo`     - echo back an argument
- `clear`    - clear the screen
- `banner`   - reprint the startup logo/banner
- `sysinfo`  - architecture, mode, and uptime info
- `credits`  - toolchain/author credits
- `ver`      - kernel version string
- `mem`      - describe the current (static) memory layout
- `cpuid`    - execute the cpuid instruction, print the real CPU vendor string
- `time`     - read the current time from the CMOS real-time clock
- `colors`   - display the full 16-color VGA palette
- `peek`     - read a byte from a physical memory address, e.g. `peek b8000`
- `stack`    - print the current stack pointer (esp)
- `uptime`   - display a tick counter
- `halt`     - halt the cpu until the next interrupt
- `shutdown` - power off the machine via the QEMU ACPI port
- `reboot`   - triple-fault the CPU to reset the machine

## Layout

- `boot/boot.asm`     - 16-bit real-mode bootloader; loads the kernel from disk, sets up a GDT, enters 32-bit protected mode
- `kernel/entry.asm`  - kernel entry stub, sets up the stack, calls into C++
- `kernel/kernel.cpp` - freestanding C++ kernel: VGA text driver (with hardware cursor sync), PS/2 keyboard driver, CMOS RTC driver, PC speaker driver, shell
- `kernel/linker.ld`  - links the kernel to run at the physical address it's loaded to (0x10000)
