#include <array>
#include <assembler/assember.hpp>
#include <cpu/cpu.hpp>
#include <cpu/opcode.hpp>
#include <emu_core/clock.hpp>
#include <emu_core/memory.hpp>
#include <emu_core/program.hpp>
#include <gtest/gtest.h>

using namespace std::string_literals;

const static std::array<uint8_t, 256> crc8_table = {
    0xea, 0xd4, 0x96, 0xa8, 0x12, 0x2c, 0x6e, 0x50, 0x7f, 0x41, 0x03, 0x3d, 0x87, 0xb9, 0xfb, 0xc5, 0xa5, 0x9b, 0xd9,
    0xe7, 0x5d, 0x63, 0x21, 0x1f, 0x30, 0x0e, 0x4c, 0x72, 0xc8, 0xf6, 0xb4, 0x8a, 0x74, 0x4a, 0x08, 0x36, 0x8c, 0xb2,
    0xf0, 0xce, 0xe1, 0xdf, 0x9d, 0xa3, 0x19, 0x27, 0x65, 0x5b, 0x3b, 0x05, 0x47, 0x79, 0xc3, 0xfd, 0xbf, 0x81, 0xae,
    0x90, 0xd2, 0xec, 0x56, 0x68, 0x2a, 0x14, 0xb3, 0x8d, 0xcf, 0xf1, 0x4b, 0x75, 0x37, 0x09, 0x26, 0x18, 0x5a, 0x64,
    0xde, 0xe0, 0xa2, 0x9c, 0xfc, 0xc2, 0x80, 0xbe, 0x04, 0x3a, 0x78, 0x46, 0x69, 0x57, 0x15, 0x2b, 0x91, 0xaf, 0xed,
    0xd3, 0x2d, 0x13, 0x51, 0x6f, 0xd5, 0xeb, 0xa9, 0x97, 0xb8, 0x86, 0xc4, 0xfa, 0x40, 0x7e, 0x3c, 0x02, 0x62, 0x5c,
    0x1e, 0x20, 0x9a, 0xa4, 0xe6, 0xd8, 0xf7, 0xc9, 0x8b, 0xb5, 0x0f, 0x31, 0x73, 0x4d, 0x58, 0x66, 0x24, 0x1a, 0xa0,
    0x9e, 0xdc, 0xe2, 0xcd, 0xf3, 0xb1, 0x8f, 0x35, 0x0b, 0x49, 0x77, 0x17, 0x29, 0x6b, 0x55, 0xef, 0xd1, 0x93, 0xad,
    0x82, 0xbc, 0xfe, 0xc0, 0x7a, 0x44, 0x06, 0x38, 0xc6, 0xf8, 0xba, 0x84, 0x3e, 0x00, 0x42, 0x7c, 0x53, 0x6d, 0x2f,
    0x11, 0xab, 0x95, 0xd7, 0xe9, 0x89, 0xb7, 0xf5, 0xcb, 0x71, 0x4f, 0x0d, 0x33, 0x1c, 0x22, 0x60, 0x5e, 0xe4, 0xda,
    0x98, 0xa6, 0x01, 0x3f, 0x7d, 0x43, 0xf9, 0xc7, 0x85, 0xbb, 0x94, 0xaa, 0xe8, 0xd6, 0x6c, 0x52, 0x10, 0x2e, 0x4e,
    0x70, 0x32, 0x0c, 0xb6, 0x88, 0xca, 0xf4, 0xdb, 0xe5, 0xa7, 0x99, 0x23, 0x1d, 0x5f, 0x61, 0x9f, 0xa1, 0xe3, 0xdd,
    0x67, 0x59, 0x1b, 0x25, 0x0a, 0x34, 0x76, 0x48, 0xf2, 0xcc, 0x8e, 0xb0, 0xd0, 0xee, 0xac, 0x92, 0x28, 0x16, 0x54,
    0x6a, 0x45, 0x7b, 0x39, 0x07, 0xbd, 0x83, 0xc1, 0xff};

template <typename iterable>
uint8_t crc8(const iterable &container) {
    uint8_t crc = 0;
    for (auto data : container) {
        crc = crc8_table[crc ^ data];
    }
    return crc;
}

class CT : public ::testing::Test {
public:
    using MemPtr = emu::MemPtr;

    emu::Memory memory;
    emu::cpu::Cpu6502 cpu;
    emu::Clock clock;

    std::array<uint8_t, 128> test_data;

    CT() {
        cpu.memory = &memory;
        cpu.clock = &clock;
        memory.clock = &clock;

        std::generate(test_data.begin(), test_data.end(), &CT::RandomByte);
    }

    static uint8_t RandomByte() { return rand() & 0xFF; }

    template <typename iterable>
    static std::string to_hex_array(const iterable &container) {
        std::string hex_table;
        for (auto v : container) {
            if (!hex_table.empty()) {
                hex_table += ",";
            }
            hex_table += fmt::format("0x{:02x}", v);
        }
        return hex_table;
    }
};

TEST_F(CT, crc8) {
    auto code = R"==(
.org 0x2000
START:
    NOP

CRC8_INIT:
    LDX #$00
    LDA #$00

CRC8_LOOP:
    CPX TEST_DATA_SIZE
    BEQ CRC8_FINISH

    EOR TEST_DATA,X
    TAY
    LDA CRC8_TABLE,Y

    INX
    BNE CRC8_LOOP

CRC8_FINISH:
    NOP
    STA RESULT_CRC8_VALUE
    JMP HALT

HALT:
    BRK #$00

.org 0x3000
CRC8_TABLE:
.byte {}

.org 0x4000
TEST_DATA_SIZE:
.byte 0x{:02x}
RESULT_CRC8_VALUE:
.byte 0x{:02x}

.org 0x4100
TEST_DATA:
.byte {}

)=="s;

    std::string hex_crc_table;
    for (auto v : crc8_table) {
        if (!hex_crc_table.empty()) {
            hex_crc_table += ",";
        }
        hex_crc_table += fmt::format("0x{:02x}", v);
    }

    auto final_code = fmt::format(code, to_hex_array(crc8_table), test_data.size(), 0, to_hex_array(test_data));
    std::cout << "-----------CODE---------------------\n" << final_code << "\n";

    auto program = emu::assembler::CompileString(final_code);
    std::cout << "-----------PROGRAM---------------------\n" << to_string(*program) << "\n";

    memory.WriteSparse(program->sparse_binary_code.sparse_map);
    cpu.reg.program_counter = program->labels["START"]->offset.value();

    try {
        std::cout << "-----------EXECUTION---------------------\n";
        cpu.ExecuteWithTimeout(std::chrono::seconds{1});
        FAIL() << "Exception was expected";
    } catch (const std::exception &e) {
        std::cout << "-----------HALTED---------------------\n";
        std::cout << e.what() << "\n";
    }

    auto emulated_crc = memory.ReadRange(program->labels["RESULT_CRC8_VALUE"]->offset.value(), 1);
    auto crc = std::vector<uint8_t>{crc8(test_data)};

    std::cout << "-----------RESULT---------------------\n";
    std::cout << "Cycles: " << clock.CurrentCycle() << "\n";
    std::cout << "R: " << to_hex_array(emulated_crc) << "\n";
    std::cout << "E: " << to_hex_array(crc) << "\n";
    std::cout << "-----------DONE---------------------\n";
    EXPECT_EQ(emulated_crc, crc);
}
