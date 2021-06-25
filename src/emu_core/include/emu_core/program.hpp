#pragma once

#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace emu {

using ByteVector = std::vector<uint8_t>;
using ByteString = std::basic_string<uint8_t>;

//-----------------------------------------------------------------------------

using Address_t = uint16_t;
using Offset_t = int16_t;
using NearOffset_t = int8_t;

constexpr Address_t operator"" _addr(unsigned long long n) {
    return static_cast<Address_t>(n);
}
constexpr Offset_t operator"" _off(unsigned long long n) {
    return static_cast<Offset_t>(n);
}

NearOffset_t RelativeJumpOffset(Address_t position, Address_t target);

struct RelocationInfo;

struct RelocationInfo;
enum class Segment {
    Unknown,
    ZeroPage,
    Code,
    Data,
    RoData,
    AbsoluteAddress,
};
std::string to_string(Segment mode);

struct SymbolInfo {
    std::string name;
    std::optional<Address_t> offset;
    bool imported = false;
    Segment segment = Segment::Unknown;

    bool operator==(const SymbolInfo &other) const;
};

std::string to_string(const SymbolInfo &symbol);
std::string to_string(std::weak_ptr<SymbolInfo> symbol);

using SymbolMap = std::unordered_map<std::string, std::shared_ptr<SymbolInfo>>;

//-----------------------------------------------------------------------------

enum class RelocationMode {
    Absolute,
    Relative,
    ZeroPage,
};
std::string to_string(RelocationMode rel_mode);
uint8_t RelocationSize(RelocationMode rm);

struct RelocationInfo {
    std::weak_ptr<SymbolInfo> target_symbol;
    Address_t position;
    RelocationMode mode;

    bool operator==(const RelocationInfo &other) const;
    bool operator<(const RelocationInfo &other) const;
};

std::string to_string(const RelocationInfo &relocation);
std::string to_string(std::weak_ptr<RelocationInfo> relocation);

struct RelocationInfoComp {
    bool operator()(const std::shared_ptr<RelocationInfo> &lhs,
                    const std::shared_ptr<RelocationInfo> &rhs) const {
        return (*lhs) < (*rhs);
    }
};

//-----------------------------------------------------------------------------

struct ValueAlias {
    std::string name;
    std::vector<uint8_t> value;
};

std::string to_string(const ValueAlias &value_alias);
std::string to_string(std::shared_ptr<ValueAlias> value_alias);

using AliasMap = std::unordered_map<std::string, std::shared_ptr<ValueAlias>>;

//-----------------------------------------------------------------------------

struct SparseBinaryCode {
    using MapType = std::unordered_map<Address_t, uint8_t>;
    using VectorType = std::vector<uint8_t>;
    MapType sparse_map;

    SparseBinaryCode(std::initializer_list<MapType::value_type> init)
        : sparse_map(std::move(init)) {}
    SparseBinaryCode() = default;
    SparseBinaryCode(Address_t base_address, const VectorType &bytes) {
        PutBytes(base_address, bytes);
    }
    SparseBinaryCode(const VectorType &bytes) : SparseBinaryCode(0, bytes) {}

    std::pair<Address_t, Address_t> CodeRange() const;
    void PutByte(Address_t address, uint8_t byte, bool overwrite = false);
    void PutBytes(Address_t address, const std::vector<uint8_t> &bytes,
                  bool overwrite = false);

    std::string HexDump(std::string_view line_prefix = "") const;
    ByteVector DumpMemory() const;

    bool operator==(const SparseBinaryCode &other) const;
};

//-----------------------------------------------------------------------------

struct Program {
    SparseBinaryCode sparse_binary_code;
    SymbolMap symbols;
    AliasMap aliases;
    std::set<std::shared_ptr<RelocationInfo>, RelocationInfoComp> relocations;

    bool operator==(const Program &other) const;

    void AddSymbol(std::shared_ptr<SymbolInfo> symbol);
    std::shared_ptr<SymbolInfo> FindSymbol(const std::string &name) const;

    void AddAlias(std::shared_ptr<ValueAlias> alias);
    std::shared_ptr<ValueAlias> FindAlias(const std::string &name) const;
};

std::string to_string(const Program &program);
std::ostream &operator<<(std::ostream &o, const Program &program);

} // namespace emu
