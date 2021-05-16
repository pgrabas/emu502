#pragma once

#include <cpu_6502/cpu6502.hpp>
#include <cpu_6502/opcode.hpp>
#include <emu_core/clock.hpp>
#include <emu_core/memory.hpp>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace emu::cpu6502 {

using Reg8Ptr = Reg8(Registers::*);
using Flags = Registers::Flags;

constexpr uint8_t operator"" _u8(unsigned long long n) {
    return static_cast<uint8_t>(n);
}

class BaseTest : public testing::Test {
public:
    using AddressMode = emu::cpu6502::AddressMode;
    using MemPtr = emu::MemPtr;
    using Registers = emu::cpu6502::Registers;
    using Flags = Registers::Flags;

    emu::Memory memory;
    Cpu6502 cpu;
    emu::Clock clock;
    Registers expected_regs;

    static constexpr MemPtr kBaseCodeAddress = 0x1770;
    static constexpr MemPtr kBaseDataAddress = 0xE000;

    BaseTest() {
        cpu.memory = &memory;
        cpu.clock = &clock;
        memory.clock = &clock;
    }

    uint8_t zero_page_address{0};
    uint8_t indirect_address{0};
    uint8_t target_byte{0};
    MemPtr test_address{0};
    MemPtr target_address{0};

    std::optional<uint64_t> expected_cycles;
    std::optional<uint64_t> expected_code_length;

    bool is_testing_jumps = false;
    bool random_reg_values = false;

    void SetUp() override {
        cpu.Reset();
        if (random_reg_values) {
            expected_regs.a = RandomByte();
            expected_regs.x = RandomByte();
            expected_regs.y = RandomByte();
            expected_regs.stack_pointer = RandomByte();
        } else {
            expected_regs.a = 0x10;
            expected_regs.x = 0x20;
            expected_regs.y = 0x30;
            expected_regs.stack_pointer = 0x40;
        }
        expected_regs.program_counter = kBaseCodeAddress;
        expected_regs.flags = RandomByte();
        cpu.reg = expected_regs;

        do {
            zero_page_address = RandomByte();
            indirect_address = RandomByte();
            target_byte = RandomByte();
            test_address = kBaseDataAddress | (RandomByte() & 0xF0);
            target_address = test_address;
        } while (indirect_address + expected_regs.y == zero_page_address ||
                 zero_page_address + expected_regs.x == indirect_address);

        EXPECT_TRUE(zero_page_address + expected_regs.x != indirect_address);
        EXPECT_TRUE(indirect_address + expected_regs.y != zero_page_address);
    }

    virtual void Execute(const std::vector<uint8_t> &code) {
        if (!is_testing_jumps) {
            expected_regs.program_counter += static_cast<MemPtr>(code.size());
        }

        std::cout << fmt::format(
            "SETUP target_byte=0x{:02x}; target_address={:04x} zero_page_address=0x{:02x}; indirect_address=0x{:02x}; "
            "test_address=0x{:04x};\n",
            target_byte, target_address, zero_page_address, indirect_address, test_address);
        std::cout << "CPU STATE 0: " << cpu.reg.Dump() << "\n";

        EXPECT_EQ(code.size(), expected_code_length.value_or(0));
        WriteMemory(cpu.reg.program_counter, code);

        cpu.ExecuteNextInstruction();

        std::cout << "CPU STATE 1: " << cpu.reg.Dump() << "\n";
        std::cout << "CYCLES:" << clock.CurrentCycle() << "\n";

        if (expected_cycles.has_value()) {
            EXPECT_EQ(clock.CurrentCycle(), expected_cycles.value());
        }
    }

    void TearDown() override {
        EXPECT_EQ(cpu.reg.flags, expected_regs.flags)
            << fmt::format("Expected:{} Actual:{}", expected_regs.DumpFlags(), cpu.reg.DumpFlags());

        EXPECT_EQ(expected_regs.stack_pointer, cpu.reg.stack_pointer)
            << fmt::format("Expected:{:02x} Actual:{:02x}", expected_regs.stack_pointer, cpu.reg.stack_pointer);
        EXPECT_EQ(expected_regs.x, cpu.reg.x)
            << fmt::format("Expected:{:02x} Actual:{:02x}", expected_regs.x, cpu.reg.x);
        EXPECT_EQ(expected_regs.y, cpu.reg.y)
            << fmt::format("Expected:{:02x} Actual:{:02x}", expected_regs.y, cpu.reg.y);
        EXPECT_EQ(expected_regs.a, cpu.reg.a)
            << fmt::format("Expected:{:02x} Actual:{:02x}", expected_regs.a, cpu.reg.a);

        EXPECT_EQ(expected_regs.program_counter, cpu.reg.program_counter)
            << fmt::format("Expected:{:02x} Actual:{:02x}", expected_regs.program_counter, cpu.reg.program_counter);
    }

    static std::vector<uint8_t> MakeCode(uint8_t opcode, uint16_t arg) {
        return {opcode, static_cast<uint8_t>(arg & 0xFF), static_cast<uint8_t>(arg >> 8)};
    }
    static std::vector<uint8_t> MakeCode(uint8_t opcode) { return {opcode}; }
    static std::vector<uint8_t> MakeCode(uint8_t opcode, uint8_t arg) { return {opcode, arg}; }

    std::vector<uint8_t> MakeCode(uint8_t opcode, AddressMode mode) {
        WriteTestData(mode);

        switch (mode) {
        case AddressMode::IM:
            return MakeCode(opcode, target_byte);
        case AddressMode::ABS:
        case AddressMode::ABSX:
        case AddressMode::ABSY:
            return MakeCode(opcode, test_address);
        case AddressMode::INDY:
        case AddressMode::INDX:
        case AddressMode::ZP:
        case AddressMode::ZPX:
        case AddressMode::ZPY:
            return MakeCode(opcode, zero_page_address);
        case AddressMode::ACC:
            return MakeCode(opcode);

        case AddressMode::Implied:
        case AddressMode::ABS_IND:
        case AddressMode::REL:
            EXPECT_FALSE(true) << "Not implented: " << __FUNCTION__;
            break; // TODO
        }
        throw std::runtime_error("Invalid address mode");
    }

    template <typename iterable>
    static std::string ToHexArray(const iterable &container) {
        std::string hex_table;
        for (auto v : container) {
            if (!hex_table.empty()) {
                hex_table += ",";
            }
            hex_table += fmt::format("0x{:02x}", v);
        }
        return hex_table;
    }

    void WriteMemory(MemPtr addr, const std::vector<uint8_t> &data) {
        std::cout << fmt::format("MEM WRITE: {:04x} -> {}\n", addr, ToHexArray(data));
        memory.Write(addr, data);
    }
    void VerifyMemory(MemPtr addr, const std::vector<uint8_t> &data) {
        auto content = memory.ReadRange(addr, data.size());
        EXPECT_EQ(data, content) << fmt::format("Base address: {:04x}", addr);
    }

    uint8_t RandomByte() { return rand() & 0xFF; }

    void WriteTestData(AddressMode mode) {
        switch (mode) {
        case AddressMode::IM:
        case AddressMode::ACC:
            return;
        case AddressMode::ABS:
            target_address = test_address;
            break;
        case AddressMode::ZP:
            target_address = zero_page_address;
            break;
        case AddressMode::ZPX:
            target_address = zero_page_address + expected_regs.x;
            break;
        case AddressMode::ZPY:
            target_address = zero_page_address + expected_regs.y;
            break;
        case AddressMode::ABSX:
            target_address = test_address + expected_regs.x;
            break;
        case AddressMode::ABSY:
            target_address = test_address + expected_regs.y;
            break;
        case AddressMode::INDX:
            WriteMemory(static_cast<MemPtr>(zero_page_address) + expected_regs.x, {indirect_address});
            target_address = indirect_address;
            break;
        case AddressMode::INDY:
            WriteMemory(zero_page_address, {indirect_address});
            target_address = static_cast<MemPtr>(indirect_address) + expected_regs.y;
            break;

        case AddressMode::Implied:
        case AddressMode::ABS_IND:
        case AddressMode::REL:
            EXPECT_FALSE(true) << "Not implented: " << __FUNCTION__;
            break; // TODO
        }

        WriteMemory(target_address, {target_byte});
    }
};

inline auto GenTestNameFunc(std::string caption = "") {
    if (!caption.empty()) {
        caption += "_";
    }
    return [caption = std::move(caption)](const auto &info) -> std::string {
        const auto &param = info.param;
        std::string name;

        using tuple_type = std::decay_t<decltype(param)>;
        using arg1 = std::tuple_element_t<1, tuple_type>;
        using arg2 = std::tuple_element_t<2, tuple_type>;

        if constexpr (std::is_same_v<arg1, AddressMode>) {
            if (!name.empty()) {
                name += "_";
            }
            name += to_string(std::get<1>(param));
        }
        if constexpr (std::is_same_v<arg1, const char *> || //
                      std::is_same_v<arg1, std::string> ||  //
                      std::is_same_v<arg1, std::string_view>) {
            if (!name.empty()) {
                name += "_";
            }
            name += std::get<1>(param);
        }

        if constexpr (std::is_same_v<arg2, AddressMode>) {
            if (!name.empty()) {
                name += "_";
            }
            name += to_string(std::get<2>(param));
        }

        return caption + name;
    };
}

} // namespace emu::cpu6502