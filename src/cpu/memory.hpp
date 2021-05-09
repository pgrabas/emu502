#pragma once

#include "clock.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fmt/format.h>
#include <iostream>
#include <vector>

namespace emu6502::cpu {

using MemPtr = uint16_t;

struct Memory {
    Clock *clock;

    std::array<uint8_t, 1024 * 64> mem;

    Memory() { mem.fill(0x55); }

    uint8_t Load(MemPtr address) {
        clock->WaitForNextCycle();
        auto r = mem[address];
        std::cout << fmt::format("MEM  READ [{:04x}] -> {:02x}\n", address, r);
        return r;
    }

    void Store(MemPtr address, uint8_t data) {
        clock->WaitForNextCycle();
        std::cout << fmt::format("MEM WRITE [{:04x}] <- {:02x}\n", address, data);
        mem[address] = data;
    }

    void Write(MemPtr addr, const std::vector<uint8_t> data) {
        std::copy(data.begin(), data.end(), mem.begin() + addr);
    }

    std::vector<uint8_t> ReadRange(MemPtr addr, size_t len) {
        std::vector<uint8_t> r;
        std::copy(mem.begin() + addr, mem.begin() + addr + len, std::back_inserter(r));
        return r;
    }
};

} // namespace emu6502::cpu