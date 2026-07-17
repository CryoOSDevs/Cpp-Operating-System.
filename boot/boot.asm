bits 16
org 0x7c00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [boot_drive], dl

    mov si, msg_loading
    call print16

    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    mov ah, 0x02
    mov al, 40
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:protected_start

disk_error:
    mov si, msg_error
    call print16
    jmp $

print16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp print16
.done:
    ret

msg_loading db "Cpp-OS bootloader: loading kernel...", 13, 10, 0
msg_error db "disk read failure", 13, 10, 0
boot_drive db 0

gdt_start:
gdt_null:
    dq 0
gdt_code:
    dw 0xffff
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
gdt_data:
    dw 0xffff
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

bits 32
protected_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    jmp CODE_SEG:0x10000

times 510-($-$$) db 0
dw 0xaa55
