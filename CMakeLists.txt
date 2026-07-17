cmake_minimum_required(VERSION 3.16)
project(CppOS CXX)

set(CMAKE_CXX_STANDARD 17)
find_program(NASM nasm REQUIRED)
if(NOT CMAKE_OBJCOPY)
    find_program(CMAKE_OBJCOPY objcopy REQUIRED)
endif()

set(BOOT_BIN ${CMAKE_BINARY_DIR}/boot.bin)
set(KERNEL_ENTRY_OBJ ${CMAKE_BINARY_DIR}/entry.o)
set(KERNEL_CPP_OBJ ${CMAKE_BINARY_DIR}/kernel.o)
set(KERNEL_ELF ${CMAKE_BINARY_DIR}/kernel.elf)
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/kernel.bin)
set(OS_IMAGE ${CMAKE_BINARY_DIR}/cppos.img)

add_custom_command(
    OUTPUT ${BOOT_BIN}
    COMMAND ${NASM} -f bin ${CMAKE_SOURCE_DIR}/boot/boot.asm -o ${BOOT_BIN}
    DEPENDS ${CMAKE_SOURCE_DIR}/boot/boot.asm
)

add_custom_command(
    OUTPUT ${KERNEL_ENTRY_OBJ}
    COMMAND ${NASM} -f elf32 ${CMAKE_SOURCE_DIR}/kernel/entry.asm -o ${KERNEL_ENTRY_OBJ}
    DEPENDS ${CMAKE_SOURCE_DIR}/kernel/entry.asm
)

add_custom_command(
    OUTPUT ${KERNEL_CPP_OBJ}
    COMMAND ${CMAKE_CXX_COMPILER} -m32 -ffreestanding -fno-exceptions -fno-rtti
            -fno-stack-protector -nostdlib -c ${CMAKE_SOURCE_DIR}/kernel/kernel.cpp
            -o ${KERNEL_CPP_OBJ}
    DEPENDS ${CMAKE_SOURCE_DIR}/kernel/kernel.cpp
)

add_custom_command(
    OUTPUT ${KERNEL_ELF}
    COMMAND ${CMAKE_CXX_COMPILER} -m32 -ffreestanding -nostdlib -static
            -Wl,--build-id=none
            -T ${CMAKE_SOURCE_DIR}/kernel/linker.ld
            -o ${KERNEL_ELF} ${KERNEL_ENTRY_OBJ} ${KERNEL_CPP_OBJ}
    DEPENDS ${KERNEL_ENTRY_OBJ} ${KERNEL_CPP_OBJ} ${CMAKE_SOURCE_DIR}/kernel/linker.ld
)

add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${KERNEL_ELF} ${KERNEL_BIN}
    DEPENDS ${KERNEL_ELF}
)

add_custom_command(
    OUTPUT ${OS_IMAGE}
    COMMAND cat ${BOOT_BIN} ${KERNEL_BIN} > ${OS_IMAGE}
    COMMAND truncate -s 1440k ${OS_IMAGE}
    DEPENDS ${BOOT_BIN} ${KERNEL_BIN}
)

add_custom_target(cppos ALL DEPENDS ${OS_IMAGE})

add_custom_target(run
    COMMAND qemu-system-i386 -fda ${OS_IMAGE}
    DEPENDS cppos
)
