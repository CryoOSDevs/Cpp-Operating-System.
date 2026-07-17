#include <stdint.h>
#include <stddef.h>

namespace port {

static inline uint8_t in8(uint16_t p) {
    uint8_t r;
    asm volatile ("inb %1, %0" : "=a"(r) : "Nd"(p));
    return r;
}

static inline void out8(uint16_t p, uint8_t v) {
    asm volatile ("outb %0, %1" : : "a"(v), "Nd"(p));
}

static inline void out16(uint16_t p, uint16_t v) {
    asm volatile ("outw %0, %1" : : "a"(v), "Nd"(p));
}

} // namespace port

namespace vga {

constexpr uint16_t kWidth = 80;
constexpr uint16_t kHeight = 25;
constexpr uintptr_t kBufferAddr = 0xB8000;
constexpr uint16_t kCrtIndex = 0x3D4;
constexpr uint16_t kCrtData = 0x3D5;

enum class Color : uint8_t {
    Black = 0, Blue = 1, Green = 2, Cyan = 3, DarkGrey = 8,
    LightBlue = 9, LightGreen = 10, LightCyan = 11, LightGrey = 7, White = 15
};

template <typename T>
constexpr uint8_t entry_color(T fg, T bg) {
    return static_cast<uint8_t>(fg) | (static_cast<uint8_t>(bg) << 4);
}

constexpr uint16_t make_entry(char c, uint8_t color) {
    return static_cast<uint16_t>(c) | (static_cast<uint16_t>(color) << 8);
}

class Terminal {
public:
    void init() {
        set_cursor_shape(14, 15);
        clear();
    }

    void clear() {
        for (uint16_t y = 0; y < kHeight; ++y)
            for (uint16_t x = 0; x < kWidth; ++x)
                buffer()[y * kWidth + x] = make_entry(' ', color_);
        row_ = 0; col_ = 0;
        sync_cursor();
    }

    void set_color(Color fg, Color bg) { color_ = entry_color(fg, bg); }

    void scroll() {
        for (uint16_t y = 1; y < kHeight; ++y)
            for (uint16_t x = 0; x < kWidth; ++x)
                buffer()[(y - 1) * kWidth + x] = buffer()[y * kWidth + x];
        for (uint16_t x = 0; x < kWidth; ++x)
            buffer()[(kHeight - 1) * kWidth + x] = make_entry(' ', color_);
        row_ = kHeight - 1;
    }

    void newline() {
        col_ = 0;
        if (++row_ == kHeight) scroll();
        sync_cursor();
    }

    void put(char c) {
        if (c == '\n') { newline(); return; }
        if (c == '\b') {
            if (col_ > 0) { --col_; buffer()[row_ * kWidth + col_] = make_entry(' ', color_); }
            sync_cursor();
            return;
        }
        buffer()[row_ * kWidth + col_] = make_entry(c, color_);
        if (++col_ == kWidth) newline();
        sync_cursor();
    }

    void write(const char* s) { while (*s) put(*s++); }

    void write_dec(uint32_t v) {
        char tmp[12]; int n = 0;
        if (v == 0) { put('0'); return; }
        while (v) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; }
        while (n) put(tmp[--n]);
    }

    void write_hex(uint32_t v) {
        constexpr char digits[] = "0123456789abcdef";
        put('0'); put('x');
        for (int shift = 28; shift >= 0; shift -= 4)
            put(digits[(v >> shift) & 0xF]);
    }

private:
    static volatile uint16_t* buffer() { return reinterpret_cast<volatile uint16_t*>(kBufferAddr); }

    void sync_cursor() {
        uint16_t pos = row_ * kWidth + col_;
        port::out8(kCrtIndex, 0x0F);
        port::out8(kCrtData, static_cast<uint8_t>(pos & 0xFF));
        port::out8(kCrtIndex, 0x0E);
        port::out8(kCrtData, static_cast<uint8_t>((pos >> 8) & 0xFF));
    }

    static void set_cursor_shape(uint8_t start, uint8_t end) {
        port::out8(kCrtIndex, 0x0A);
        port::out8(kCrtData, start);
        port::out8(kCrtIndex, 0x0B);
        port::out8(kCrtData, end);
    }

    uint16_t row_ = 0, col_ = 0;
    uint8_t color_ = entry_color(Color::Cyan, Color::Black);
};

Terminal term;

} // namespace vga

namespace kbd {

constexpr uint16_t kDataPort = 0x60;
constexpr uint16_t kStatusPort = 0x64;

constexpr char kLower[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

bool has_key() { return port::in8(kStatusPort) & 1; }

char read_char() {
    while (!has_key()) {}
    uint8_t code = port::in8(kDataPort);
    if (code & 0x80) return 0;
    return kLower[code & 0x7f];
}

} // namespace kbd

namespace cmos {

constexpr uint16_t kIndexPort = 0x70;
constexpr uint16_t kDataPort = 0x71;

uint8_t read(uint8_t reg) {
    port::out8(kIndexPort, reg);
    return port::in8(kDataPort);
}

uint8_t bcd_to_bin(uint8_t v) {
    return static_cast<uint8_t>((v & 0x0F) + ((v >> 4) * 10));
}

struct Time { uint8_t hours, minutes, seconds; };

Time read_time() {
    Time t{};
    t.seconds = bcd_to_bin(read(0x00));
    t.minutes = bcd_to_bin(read(0x02));
    t.hours = bcd_to_bin(read(0x04));
    return t;
}

} // namespace cmos

namespace util {

constexpr size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; ++a; ++b; }
    return *a == *b;
}

bool starts_with(const char* s, const char* prefix) {
    while (*prefix) { if (*s != *prefix) return false; ++s; ++prefix; }
    return true;
}

uint32_t parse_hex(const char* s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t v = 0;
    while (*s) {
        char c = *s++;
        uint32_t digit;
        if (c >= '0' && c <= '9') digit = static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<uint32_t>(c - 'A' + 10);
        else break;
        v = (v << 4) | digit;
    }
    return v;
}

char* split_first_token(char* s) {
    while (*s && *s != ' ') ++s;
    if (*s == ' ') { *s = 0; return s + 1; }
    return nullptr;
}

void reverse_inplace(char* s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len / 2; ++i) {
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
}

} // namespace util

namespace shell {

constexpr size_t kBufSize = 128;

struct Command {
    const char* name;
    const char* help;
};

constexpr Command kCommands[] = {
    {"help", "list available commands"},
    {"about", "display information about Cpp-OS"},
    {"echo", "echo back the given argument"},
    {"clear", "clear the terminal screen"},
    {"uptime", "display an incrementing tick counter"},
    {"banner", "reprint the startup banner"},
    {"sysinfo", "display architecture and mode information"},
    {"credits", "display author and toolchain credits"},
    {"ver", "display the kernel version string"},
    {"mem", "describe the current memory layout"},
    {"cpuid", "query and print the cpu vendor string"},
    {"time", "read the current time from the cmos rtc"},
    {"colors", "display the 16-color vga palette"},
    {"peek", "read a byte from a physical memory address"},
    {"poke", "write a byte to a physical memory address"},
    {"beep", "emit a tone through the pc speaker"},
    {"rand", "generate a pseudo-random number"},
    {"reverse", "reverse the given string"},
    {"stack", "print the current stack pointer"},
    {"halt", "halt the cpu until the next interrupt"},
    {"shutdown", "power off the machine via acpi"},
    {"reboot", "triple fault the cpu to reboot"},
};

constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(Command);
uint32_t ticks = 0;

void print_banner();

void print_prompt() {
    vga::term.set_color(vga::Color::LightCyan, vga::Color::Black);
    vga::term.write("cpp-os> ");
    vga::term.set_color(vga::Color::Cyan, vga::Color::Black);
}

void cmd_help() {
    for (size_t i = 0; i < kCommandCount; ++i) {
        vga::term.write("  ");
        vga::term.write(kCommands[i].name);
        vga::term.write(" - ");
        vga::term.write(kCommands[i].help);
        vga::term.put('\n');
    }
}

void cmd_about() {
    vga::term.write("Cpp-OS: a minimal x86 kernel written in freestanding C++.\n");
    vga::term.write("Assembled bootloader, protected mode kernel, VGA text driver.\n");
    vga::term.write("Cold. Controlled. Clean.\n");
}

void cmd_sysinfo() {
    vga::term.write("architecture : x86 (i386)\n");
    vga::term.write("cpu mode     : 32-bit protected mode\n");
    vga::term.write("video        : VGA text mode, 80x25\n");
    vga::term.write("stack        : 16 KiB, statically reserved\n");
    vga::term.write("uptime ticks : ");
    vga::term.write_dec(ticks);
    vga::term.put('\n');
}

void cmd_credits() {
    vga::term.write("Cpp-OS - built with NASM, freestanding C++17, CMake, QEMU.\n");
    vga::term.write("No libc. No mercy. Just registers and silence.\n");
}

void cmd_ver() {
    vga::term.write("Cpp-OS kernel version 0.2, i386 protected-mode build\n");
}

void cmd_mem() {
    vga::term.write("kernel loaded  : 0x00010000\n");
    vga::term.write("kernel stack   : 16 KiB, statically reserved in .bss\n");
    vga::term.write("vga framebuffer: 0x000B8000\n");
    vga::term.write("paging         : disabled, flat unpaged addressing\n");
    vga::term.write("allocator      : none, everything is static\n");
}

void cmd_cpuid() {
    uint32_t eax, ebx, ecx, edx;
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

    char vendor[13];
    __builtin_memcpy(vendor + 0, &ebx, 4);
    __builtin_memcpy(vendor + 4, &edx, 4);
    __builtin_memcpy(vendor + 8, &ecx, 4);
    vendor[12] = 0;

    vga::term.write("cpuid.max_leaf : ");
    vga::term.write_hex(eax);
    vga::term.put('\n');
    vga::term.write("vendor string  : ");
    vga::term.write(vendor);
    vga::term.put('\n');
}

void shutdown() {
    port::out16(0x604, 0x2000);
    for (;;) asm volatile ("hlt");
}

void cmd_write_dec2(uint32_t v) {
    vga::term.put(static_cast<char>('0' + (v / 10) % 10));
    vga::term.put(static_cast<char>('0' + v % 10));
}

void cmd_time() {
    cmos::Time t = cmos::read_time();
    vga::term.write("cmos rtc time  : ");
    cmd_write_dec2(t.hours);
    vga::term.put(':');
    cmd_write_dec2(t.minutes);
    vga::term.put(':');
    cmd_write_dec2(t.seconds);
    vga::term.write(" (utc offset unknown)\n");
}

void cmd_colors() {
    for (uint8_t bg = 0; bg < 16; ++bg) {
        vga::term.set_color(static_cast<vga::Color>(15 - bg), static_cast<vga::Color>(bg));
        vga::term.write_hex(bg);
        vga::term.write(" ## ");
    }
    vga::term.set_color(vga::Color::Cyan, vga::Color::Black);
    vga::term.put('\n');
}

void cmd_peek(const char* arg) {
    if (!arg || util::strlen(arg) == 0) {
        vga::term.write("usage: peek <hex address>\n");
        return;
    }
    uint32_t addr = util::parse_hex(arg);
    volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(addr);
    vga::term.write_hex(addr);
    vga::term.write(" -> ");
    vga::term.write_hex(*ptr);
    vga::term.put('\n');
}

void cmd_poke(char* arg) {
    if (!arg || util::strlen(arg) == 0) {
        vga::term.write("usage: poke <hex address> <hex byte>\n");
        return;
    }
    char* value_str = util::split_first_token(arg);
    if (!value_str) {
        vga::term.write("usage: poke <hex address> <hex byte>\n");
        return;
    }
    uint32_t addr = util::parse_hex(arg);
    uint8_t value = static_cast<uint8_t>(util::parse_hex(value_str));
    volatile uint8_t* ptr = reinterpret_cast<volatile uint8_t*>(addr);
    *ptr = value;
    vga::term.write_hex(addr);
    vga::term.write(" <- ");
    vga::term.write_hex(value);
    vga::term.put('\n');
}

void beep() {
    constexpr uint32_t kPitFrequency = 1193180;
    constexpr uint32_t kToneHz = 1000;
    uint16_t divisor = static_cast<uint16_t>(kPitFrequency / kToneHz);

    port::out8(0x43, 0xB6);
    port::out8(0x42, static_cast<uint8_t>(divisor & 0xFF));
    port::out8(0x42, static_cast<uint8_t>((divisor >> 8) & 0xFF));

    uint8_t speaker = port::in8(0x61);
    port::out8(0x61, speaker | 0x03);

    for (volatile uint32_t i = 0; i < 4000000; ++i) {}

    port::out8(0x61, speaker & 0xFC);
}

void cmd_beep() {
    vga::term.write("emitting 1000 Hz tone...\n");
    beep();
}

uint32_t rand_next() {
    static uint32_t state = 88172645463325252u & 0xFFFFFFFFu;
    state ^= ticks * 2654435761u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void cmd_rand() {
    vga::term.write("random: ");
    vga::term.write_dec(rand_next() % 1000000);
    vga::term.put('\n');
}

void cmd_reverse(char* arg) {
    if (!arg || util::strlen(arg) == 0) {
        vga::term.write("usage: reverse <text>\n");
        return;
    }
    util::reverse_inplace(arg);
    vga::term.write(arg);
    vga::term.put('\n');
}

void cmd_stack() {
    uint32_t esp;
    asm volatile ("mov %%esp, %0" : "=r"(esp));
    vga::term.write("current esp : ");
    vga::term.write_hex(esp);
    vga::term.put('\n');
}

void cmd_echo(const char* arg) {
    vga::term.write(arg);
    vga::term.put('\n');
}

void halt_once() {
    asm volatile ("hlt");
}

void reboot() {
    struct [[gnu::packed]] Idtr { uint16_t limit; uint32_t base; };
    static const Idtr zero{0, 0};
    asm volatile ("lidt %0" : : "m"(zero));
    asm volatile ("int $0x03");
}

void dispatch(char* line) {
    char* arg = line;
    while (*arg && *arg != ' ') ++arg;
    if (*arg == ' ') { *arg = 0; ++arg; } else { arg = nullptr; }

    if (util::strlen(line) == 0) return;

    if (util::streq(line, "help")) { cmd_help(); return; }
    if (util::streq(line, "about")) { cmd_about(); return; }
    if (util::streq(line, "clear")) { vga::term.clear(); return; }
    if (util::streq(line, "banner")) { print_banner(); return; }
    if (util::streq(line, "sysinfo")) { cmd_sysinfo(); return; }
    if (util::streq(line, "credits")) { cmd_credits(); return; }
    if (util::streq(line, "ver")) { cmd_ver(); return; }
    if (util::streq(line, "mem")) { cmd_mem(); return; }
    if (util::streq(line, "cpuid")) { cmd_cpuid(); return; }
    if (util::streq(line, "time")) { cmd_time(); return; }
    if (util::streq(line, "colors")) { cmd_colors(); return; }
    if (util::streq(line, "peek")) { cmd_peek(arg); return; }
    if (util::streq(line, "poke")) { cmd_poke(arg); return; }
    if (util::streq(line, "beep")) { cmd_beep(); return; }
    if (util::streq(line, "rand")) { cmd_rand(); return; }
    if (util::streq(line, "reverse")) { cmd_reverse(arg); return; }
    if (util::streq(line, "stack")) { cmd_stack(); return; }
    if (util::streq(line, "shutdown")) { shutdown(); return; }
    if (util::streq(line, "halt")) { halt_once(); return; }
    if (util::streq(line, "uptime")) {
        vga::term.write("ticks: ");
        vga::term.write_dec(ticks);
        vga::term.put('\n');
        return;
    }
    if (util::streq(line, "reboot")) { reboot(); return; }
    if (util::streq(line, "echo")) {
        cmd_echo(arg ? arg : "");
        return;
    }

    vga::term.write("unknown command: ");
    vga::term.write(line);
    vga::term.put('\n');
}

void run() {
    char buf[kBufSize];
    size_t len = 0;

    print_prompt();

    for (;;) {
        ++ticks;
        char c = kbd::read_char();
        if (!c) continue;

        if (c == '\n') {
            vga::term.put('\n');
            buf[len] = 0;
            dispatch(buf);
            len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (len > 0) { --len; vga::term.put('\b'); }
        } else if (len + 1 < kBufSize) {
            buf[len++] = c;
            vga::term.put(c);
        }
    }
}

void print_banner() {
    vga::term.set_color(vga::Color::DarkGrey, vga::Color::Black);

    const char* logo[] = {
        " ______            ______  _____ ",
        "/ ____/___  ____  / __ \\/ ___/",
        "/ /   / __ \\/ __ \\/ / / /\\__ \\ ",
        "/ /___/ /_/ / /_/ / /_/ /___/ / ",
        "\\____/ .___/ .___/\\____//____/  ",
        "    /_/   /_/                   ",
    };

    for (const char* l : logo) { vga::term.write(l); vga::term.put('\n'); }
    vga::term.put('\n');
    vga::term.set_color(vga::Color::LightGrey, vga::Color::Black);
    vga::term.write("      [ Cold. Controlled. Clean. ]\n\n");
    vga::term.set_color(vga::Color::Cyan, vga::Color::Black);
    vga::term.write("type 'help' for a list of commands\n\n");
}

} // namespace shell

extern "C" void kernel_main() {
    vga::term.init();
    shell::print_banner();
    shell::run();
}
