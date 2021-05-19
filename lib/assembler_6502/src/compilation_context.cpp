#include "compilation_context.hpp"
#include "byte_utils.hpp"
#include "instruction_argument.hpp"

namespace emu::assembler6502 {

using AddressMode = cpu6502::AddressMode;
using OpcodeInfo = cpu6502::OpcodeInfo;

const std::unordered_map<std::string, CommandParsingInfo> CompilationContext::kCommandParseInfo = {
    {"byte", {&CompilationContext::ParseByteCommand}},
    {"word", {&CompilationContext::ParseWordCommand}},
    {"org", {&CompilationContext::ParseOriginCommand}},
};

void CompilationContext::ParseByteCommand(LineTokenizer &tokenizer) {
    while (tokenizer.HasInput()) {
        auto tok = tokenizer.NextToken();
        program.sparse_binary_code.PutBytes(current_position, ToBytes(ParseByte(tok.value)));
        ++current_position;
    }
}

void CompilationContext::ParseWordCommand(LineTokenizer &tokenizer) {
    while (tokenizer.HasInput()) {
        auto tok = tokenizer.NextToken();
        program.sparse_binary_code.PutBytes(current_position, ToBytes(ParseWord(tok.value)));
        current_position += 2;
    }
}

void CompilationContext::ParseOriginCommand(LineTokenizer &tokenizer) {
    auto tok = tokenizer.NextToken();
    auto new_pos = ParseWord(tok.value);
    Log("Setting position {:04x} -> {:04x}", current_position, new_pos);
    current_position = new_pos;
}

void CompilationContext::AddLabel(const std::string &name) {
    if (auto it = program.labels.find(name); it == program.labels.end()) {
        Log("Adding label '{}' at {:04x}", name, current_position);
        auto l = LabelInfo{
            .name = name,
            .offset = current_position,
            .imported = false,
        };
        program.labels[l.name] = std::make_shared<LabelInfo>(l);
    } else {
        Log("Found label '{}' at {:04x}", name, current_position);
        it->second->imported = false;
        if (it->second->offset.has_value()) {
            Exception("Label {} is already defined", name);
        }

        it->second->offset = current_position;
        RelocateLabel(*it->second);
    }
}

void CompilationContext::RelocateLabel(const LabelInfo &label_info) {
    for (auto weak_rel : label_info.label_references) {
        auto rel = weak_rel.lock();
        Log("Relocating reference to {} at {}", label_info.name, to_string(*rel));
        if (rel->mode == RelocationMode::Absolute) {
            auto bytes = ToBytes(current_position);
            program.sparse_binary_code.PutBytes(rel->position, bytes, true);
        } else {
            auto bytes = ToBytes(RelativeJumpOffset(rel->position + 1, current_position));
            program.sparse_binary_code.PutBytes(rel->position, bytes, true);
        }
    }
}

void CompilationContext::ParseInstruction(LineTokenizer &tokenizer, const InstructionParsingInfo &instruction) {
    auto first_token = tokenizer.NextToken();
    auto token = first_token.String();

    if (auto next = tokenizer.NextToken(); next) {
        token += ",";
        token += next.String();
    }

    auto argument = ParseInstructionArgument(token);

    std::vector<AddressMode> instruction_address_modes;
    for (auto &[f, s] : instruction.variants) {
        instruction_address_modes.emplace_back(f);
    }
    std::sort(instruction_address_modes.begin(), instruction_address_modes.end());
    std::sort(argument.possible_address_modes.begin(), argument.possible_address_modes.end());

    std::set<AddressMode> address_modes;

    std::set_intersection(argument.possible_address_modes.begin(), argument.possible_address_modes.end(), //
                          instruction_address_modes.begin(), instruction_address_modes.end(),             //
                          std::inserter(address_modes, address_modes.begin()));

    AddressMode selected_mode = std::visit(
        [&](auto &arg_value) -> AddressMode { return SelectInstuctionVariant(address_modes, instruction, arg_value); },
        argument.argument_value);

    try {
        auto &opcode = instruction.variants.at(selected_mode);
        program.sparse_binary_code.PutByte(current_position++, opcode.opcode);
        std::visit([&](auto &arg_value) { ProcessInstructionArgument(opcode, arg_value); }, argument.argument_value);
    } catch (const std::exception &e) {
        throw std::runtime_error("opcode does not support mode [TODO] " + std::string(e.what()));
    }
}

AddressMode CompilationContext::SelectInstuctionVariant(const std::set<AddressMode> &modes,
                                                        const InstructionParsingInfo &instruction, std::nullptr_t) {
    if (modes.size() != 1 || *modes.begin() != AddressMode::Implied) {
        throw std::runtime_error("Failed to select implied variant");
    }
    return AddressMode::Implied;
}

AddressMode CompilationContext::SelectInstuctionVariant(std::set<AddressMode> modes,
                                                        const InstructionParsingInfo &instruction, std::string label) {
    modes.erase(AddressMode::ZPX); // TODO: not yet supported for labels/aliases
    modes.erase(AddressMode::ZPY);
    modes.erase(AddressMode::ZP);
    if (modes.size() != 1) {
        throw std::runtime_error("Failed to select label variant");
    }
    return *modes.begin();
}

AddressMode CompilationContext::SelectInstuctionVariant(const std::set<AddressMode> &modes,
                                                        const InstructionParsingInfo &instruction,
                                                        std::vector<uint8_t> data) {
    if (modes.size() != 1) {
        throw std::runtime_error("Failed to select data variant");
    }
    return *modes.begin();
}

void CompilationContext::ProcessInstructionArgument(const OpcodeInfo &opcode, std::string label) {
    auto relocation = std::make_shared<RelocationInfo>();
    relocation->position = current_position;

    auto existing_it = program.labels.find(label);
    if (existing_it == program.labels.end()) {
        std::cout << "ADDING LABEL FORWARD REF " << label << " at " << current_position << "\n";
        auto l = LabelInfo{
            .name = label,
            .imported = true,
            .label_references = {relocation},
        };
        program.labels[l.name] = std::make_shared<LabelInfo>(l);
        existing_it = program.labels.find(label);
    } else {
        std::cout << "ADDING LABEL REF " << label << " at " << current_position << "\n";
        existing_it->second->label_references.emplace_back(relocation);
    }

    relocation->target_label = existing_it->second;
    auto label_addr = existing_it->second->offset.value_or(current_position);
    if (opcode.addres_mode == AddressMode::REL) {
        auto bytes = ToBytes(RelativeJumpOffset(current_position + 1, label_addr));
        relocation->mode = RelocationMode::Relative;
        ProcessInstructionArgument(opcode, bytes);
    } else {
        relocation->mode = RelocationMode::Absolute;
        ProcessInstructionArgument(opcode, ToBytes(label_addr));
    }

    program.relocations.insert(relocation);
}

void CompilationContext::ProcessInstructionArgument(const OpcodeInfo &opcode, std::nullptr_t) {
}

void CompilationContext::ProcessInstructionArgument(const OpcodeInfo &opcode, std::vector<uint8_t> data) {
    program.sparse_binary_code.PutBytes(current_position, data);
    current_position += data.size();
}

} // namespace emu::assembler6502
