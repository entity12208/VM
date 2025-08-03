#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <cstdint>

constexpr size_t RAM_SIZE = 20 * 1024 * 1024;    // 20 MB
constexpr size_t DISK_SIZE = 100 * 1024 * 1024;  // 100 MB
const std::string DISK_FILE = "virtual_disk.bin";

class RAM {
public:
    std::vector<uint8_t> memory;

    RAM() : memory(RAM_SIZE, 0) {}

    uint8_t read(size_t addr) const {
        assert(addr < RAM_SIZE);
        return memory[addr];
    }

    void write(size_t addr, uint8_t value) {
        assert(addr < RAM_SIZE);
        memory[addr] = value;
    }
};

class Disk {
public:
    std::fstream file;

    Disk() {
        std::ifstream infile(DISK_FILE, std::ios::binary);
        if (!infile.good()) {
            std::ofstream create(DISK_FILE, std::ios::binary);
            std::vector<char> zeros(DISK_SIZE, 0);
            create.write(zeros.data(), DISK_SIZE);
            create.close();
        }
        file.open(DISK_FILE, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open disk file." << std::endl;
            std::exit(1);
        }
    }

    uint8_t read(size_t addr) {
        assert(addr < DISK_SIZE);
        file.seekg(addr);
        char value;
        file.read(&value, 1);
        return static_cast<uint8_t>(value);
    }

    void write(size_t addr, uint8_t value) {
        assert(addr < DISK_SIZE);
        file.seekp(addr);
        char byte = static_cast<char>(value);
        file.write(&byte, 1);
        file.flush();
    }

    ~Disk() {
        if (file.is_open()) file.close();
    }
};

class CPU {
public:
    RAM& ram;
    Disk& disk;

    uint8_t regs[4] = {0};    // 4 general purpose 8-bit registers: R0-R3
    uint16_t pc = 0;          // Program Counter (16-bit)
    uint8_t sp = 0xFF;        // Stack Pointer (stack grows down, in RAM)
    bool zf = false;          // Zero Flag
    bool running = false;

    CPU(RAM& r, Disk& d) : ram(r), disk(d) {}

    uint8_t fetch() {
        assert(pc < RAM_SIZE);
        return ram.read(pc++);
    }

    void push(uint8_t val) {
        ram.write(sp--, val);
    }

    uint8_t pop() {
        return ram.read(++sp);
    }

    void execute() {
        running = true;
        while (running) {
            uint8_t instr = fetch();
            uint8_t opcode = instr >> 4;
            uint8_t operand = instr & 0x0F;

            switch (opcode) {
                case 0x0: // NOP
                    break;

                case 0x1: { // LOAD Rn, imm8
                    uint8_t imm = fetch();
                    regs[operand] = imm;
                    zf = (regs[operand] == 0);
                    break;
                }

                case 0x2: { // STORE Rn, addr8
                    uint8_t addr = fetch();
                    ram.write(addr, regs[operand]);
                    break;
                }

                case 0x3: { // ADD Rn, Rm
                    uint8_t reg_m = fetch();
                    uint16_t res = regs[operand] + regs[reg_m];
                    regs[operand] = res & 0xFF;
                    zf = (regs[operand] == 0);
                    break;
                }

                case 0x4: { // SUB Rn, Rm
                    uint8_t reg_m = fetch();
                    int16_t res = regs[operand] - regs[reg_m];
                    regs[operand] = res & 0xFF;
                    zf = (regs[operand] == 0);
                    break;
                }

                case 0x5: { // JMP addr16
                    uint8_t low = fetch();
                    uint8_t high = fetch();
                    pc = (static_cast<uint16_t>(high) << 8) | low;
                    break;
                }

                case 0x6: { // JZ addr16
                    uint8_t low = fetch();
                    uint8_t high = fetch();
                    uint16_t addr = (static_cast<uint16_t>(high) << 8) | low;
                    if (zf) pc = addr;
                    break;
                }

                case 0x7: { // CALL addr16
                    uint8_t low = fetch();
                    uint8_t high = fetch();
                    uint16_t addr = (static_cast<uint16_t>(high) << 8) | low;
                    push((pc >> 8) & 0xFF);
                    push(pc & 0xFF);
                    pc = addr;
                    break;
                }

                case 0x8: { // RET
                    uint8_t low = pop();
                    uint8_t high = pop();
                    pc = (static_cast<uint16_t>(high) << 8) | low;
                    break;
                }

                case 0x9: { // IN Rn
                    int val;
                    std::cout << "Input value for R" << (int)operand << ": ";
                    std::cin >> val;
                    regs[operand] = val & 0xFF;
                    zf = (regs[operand] == 0);
                    break;
                }

                case 0xA: { // OUT Rn
                    std::cout << "Output R" << (int)operand << ": " << (int)regs[operand] << std::endl;
                    break;
                }

                case 0xB: { // DISK_READ Rn, addr16
                    uint8_t low = fetch();
                    uint8_t high = fetch();
                    uint16_t addr = (static_cast<uint16_t>(high) << 8) | low;
                    regs[operand] = disk.read(addr);
                    zf = (regs[operand] == 0);
                    break;
                }

                case 0xC: { // DISK_WRITE Rn, addr16
                    uint8_t low = fetch();
                    uint8_t high = fetch();
                    uint16_t addr = (static_cast<uint16_t>(high) << 8) | low;
                    disk.write(addr, regs[operand]);
                    break;
                }

                case 0xF: { // HALT
                    running = false;
                    break;
                }

                default:
                    std::cerr << "Unknown opcode: 0x" << std::hex << (int)opcode << std::endl;
                    running = false;
                    break;
            }
        }
    }
};

int main() {
    RAM ram;
    Disk disk;
    CPU cpu(ram, disk);

    std::cout << "Virtual Machine initialized.\n";
    std::cout << "Load your program into RAM (e.g., ram.write(addr, byte)) and set cpu.pc then call cpu.execute().\n";

    // Example:
    // cpu.pc = 0;
    // ram.write(0, ...); // Load program bytes
    // cpu.execute();

    return 0;
}
