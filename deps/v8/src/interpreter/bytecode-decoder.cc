// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-decoder.h"

#include <iomanip>

#include "src/utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

// static
Register BytecodeDecoder::DecodeRegisterOperand(const uint8_t* operand_start,
                                                OperandType operand_type,
                                                OperandScale operand_scale) {
  DCHECK(Bytecodes::IsRegisterOperandType(operand_type));
  int32_t operand =
      DecodeSignedOperand(operand_start, operand_type, operand_scale);
  return Register::FromOperand(operand);
}

// static
int32_t BytecodeDecoder::DecodeSignedOperand(const uint8_t* operand_start,
                                             OperandType operand_type,
                                             OperandScale operand_scale) {
  DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return static_cast<int8_t>(*operand_start);
    case OperandSize::kShort:
      return static_cast<int16_t>(ReadUnalignedUInt16(operand_start));
    case OperandSize::kQuad:
      return static_cast<int32_t>(ReadUnalignedUInt32(operand_start));
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
uint32_t BytecodeDecoder::DecodeUnsignedOperand(const uint8_t* operand_start,
                                                OperandType operand_type,
                                                OperandScale operand_scale) {
  DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return *operand_start;
    case OperandSize::kShort:
      return ReadUnalignedUInt16(operand_start);
    case OperandSize::kQuad:
      return ReadUnalignedUInt32(operand_start);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
std::ostream& BytecodeDecoder::Decode(std::ostream& os,
                                      const uint8_t* bytecode_start,
                                      int parameter_count) {
  Bytecode bytecode = Bytecodes::FromByte(bytecode_start[0]);
  int prefix_offset = 0;
  OperandScale operand_scale = OperandScale::kSingle;
  if (Bytecodes::IsPrefixScalingBytecode(bytecode)) {
    prefix_offset = 1;
    operand_scale = Bytecodes::PrefixBytecodeToOperandScale(bytecode);
    bytecode = Bytecodes::FromByte(bytecode_start[1]);
  }

  // Prepare to print bytecode and operands as hex digits.
  std::ios saved_format(nullptr);
  saved_format.copyfmt(saved_format);
  os.fill('0');
  os.flags(std::ios::hex);

  int bytecode_size = Bytecodes::Size(bytecode, operand_scale);
  for (int i = 0; i < prefix_offset + bytecode_size; i++) {
    os << std::setw(2) << static_cast<uint32_t>(bytecode_start[i]) << ' ';
  }
  os.copyfmt(saved_format);

  const int kBytecodeColumnSize = 6;
  for (int i = prefix_offset + bytecode_size; i < kBytecodeColumnSize; i++) {
    os << "   ";
  }

  os << Bytecodes::ToString(bytecode, operand_scale) << " ";

  // Operands for the debug break are from the original instruction.
  if (Bytecodes::IsDebugBreak(bytecode)) return os;

  int number_of_operands = Bytecodes::NumberOfOperands(bytecode);
  int range = 0;
  for (int i = 0; i < number_of_operands; i++) {
    OperandType op_type = Bytecodes::GetOperandType(bytecode, i);
    int operand_offset =
        Bytecodes::GetOperandOffset(bytecode, i, operand_scale);
    const uint8_t* operand_start =
        &bytecode_start[prefix_offset + operand_offset];
    switch (op_type) {
      case interpreter::OperandType::kRegCount:
        os << "#"
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        break;
      case interpreter::OperandType::kIdx:
      case interpreter::OperandType::kRuntimeId:
      case interpreter::OperandType::kIntrinsicId:
        os << "["
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kImm:
        os << "[" << DecodeSignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kFlag8:
        os << "#"
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        break;
      case interpreter::OperandType::kMaybeReg:
      case interpreter::OperandType::kReg:
      case interpreter::OperandType::kRegOut: {
        Register reg =
            DecodeRegisterOperand(operand_start, op_type, operand_scale);
        os << reg.ToString(parameter_count);
        break;
      }
      case interpreter::OperandType::kRegOutTriple:
        range += 1;
      case interpreter::OperandType::kRegOutPair:
      case interpreter::OperandType::kRegPair: {
        range += 1;
        Register first_reg =
            DecodeRegisterOperand(operand_start, op_type, operand_scale);
        Register last_reg = Register(first_reg.index() + range);
        os << first_reg.ToString(parameter_count) << "-"
           << last_reg.ToString(parameter_count);
        break;
      }
      case interpreter::OperandType::kNone:
        UNREACHABLE();
        break;
    }
    if (i != number_of_operands - 1) {
      os << ", ";
    }
  }
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
