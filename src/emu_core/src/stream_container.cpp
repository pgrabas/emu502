
#include "emu_core/stream_container.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <stdexcept>

namespace emu {

std::istream *StreamContainer::OpenInput(const std::string &file, bool is_binary) {
    if (file == "-") {
        if (is_binary) {
            //TODO:?
        }
        return &std::cin;
    }

    if (std::filesystem::is_regular_file(file)) {
        auto f = std::make_shared<std::ifstream>(file, std::ios::in | (is_binary ? std::ios::binary : 0));
        f->exceptions(std::ifstream::badbit);
        input_streams.emplace_back(f);
        return f.get();
    }

    throw std::runtime_error(fmt::format("Input file '{}' is not valid"));
}

std::ostream *StreamContainer::OpenOutput(const std::string &file, bool is_binary) {
    if (file == "-") {
        if (is_binary) {
            //TODO:?
        }
        return &std::cout;
    }

    auto f = std::make_shared<std::ofstream>(file, std::ios::out | (is_binary ? std::ios::binary : 0));
    f->exceptions(std::ifstream::badbit);
    output_streams.emplace_back(f);
    return f.get();
}

} // namespace emu