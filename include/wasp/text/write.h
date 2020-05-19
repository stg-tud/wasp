//
// Copyright 2020 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef WASP_TEXT_WRITE_H_
#define WASP_TEXT_WRITE_H_

#include <algorithm>
#include <cassert>
#include <string>
#include <type_traits>

#include "wasp/base/format.h"
#include "wasp/base/formatters.h"
#include "wasp/base/types.h"
#include "wasp/base/v128.h"
#include "wasp/text/numeric.h"
#include "wasp/text/types.h"

namespace wasp {
namespace text {

// TODO: Rename to Context and put in write namespace?
struct WriteContext {
  void ClearSeparator() { separator = ""; }
  void Space() { separator = " "; }
  void Newline() { separator = indent; }

  void Indent() { indent += "  "; }
  void Dedent() { indent.erase(indent.size() - 2); }

  std::string separator;
  std::string indent = "\n";
  Base base = Base::Decimal;
};

// WriteRaw
template <typename Iterator>
Iterator WriteRaw(WriteContext& context, char value, Iterator out) {
  *out++ = value;
  return out;
}

template <typename Iterator>
Iterator WriteRaw(WriteContext& context, string_view value, Iterator out) {
  return std::copy(value.begin(), value.end(), out);
}

template <typename Iterator>
Iterator WriteRaw(WriteContext& context,
                  const std::string& value,
                  Iterator out) {
  return std::copy(value.begin(), value.end(), out);
}

template <typename Iterator>
Iterator WriteSeparator(WriteContext& context, Iterator out) {
  out = WriteRaw(context, context.separator, out);
  context.ClearSeparator();
  return out;
}

// WriteFormat
template <typename Iterator, typename T>
Iterator WriteFormat(WriteContext& context,
                     const T& value,
                     Iterator out,
                     string_view format_string = "{}") {
  out = WriteSeparator(context, out);
  out = WriteRaw(context, format(format_string, value), out);
  context.Space();
  return out;
}

template <typename Iterator>
Iterator WriteLpar(WriteContext& context, Iterator out) {
  out = WriteSeparator(context, out);
  out = WriteRaw(context, '(', out);
  return out;
}

template <typename Iterator>
Iterator WriteLpar(WriteContext& context, string_view name, Iterator out) {
  out = WriteLpar(context, out);
  out = WriteRaw(context, name, out);
  context.Space();
  return out;
}

template <typename Iterator>
Iterator WriteRpar(WriteContext& context, Iterator out) {
  context.ClearSeparator();
  out = WriteRaw(context, ')', out);
  context.Space();
  return out;
}

template <typename Iterator, typename SourceIter>
Iterator WriteRange(WriteContext& context,
                    SourceIter begin,
                    SourceIter end,
                    Iterator out) {
  for (auto iter = begin; iter != end; ++iter) {
    out = Write(context, *iter, out);
  }
  return out;
}

template <typename Iterator, typename T>
Iterator WriteVector(WriteContext& context,
                     const std::vector<T>& values,
                     Iterator out) {
  return WriteRange(context, values.begin(), values.end(), out);
}

template <typename Iterator, typename T>
Iterator Write(WriteContext& context,
               const optional<T>& value_opt,
               Iterator out) {
  if (value_opt) {
    out = Write(context, *value_opt, out);
  }
  return out;
}

template <typename Iterator, typename T>
Iterator Write(WriteContext& context, const At<T>& value, Iterator out) {
  return Write(context, *value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, string_view value, Iterator out) {
  out = WriteSeparator(context, out);
  out = WriteRaw(context, value, out);
  context.Space();
  return out;
}

template <typename Iterator, typename T>
Iterator WriteNat(WriteContext& context, T value, Iterator out) {
  return Write(context, string_view{NatToStr<T>(value, context.base)}, out);
}

template <typename Iterator,
          typename T,
          typename std::enable_if_t<std::is_integral_v<T>, int> = 0>
Iterator Write(WriteContext& context, T value, Iterator out) {
  return Write(context, string_view{IntToStr<T>(value, context.base)}, out);
}

template <typename Iterator,
          typename T,
          typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
Iterator Write(WriteContext& context, T value, Iterator out) {
  return Write(context, string_view{FloatToStr<T>(value, context.base)}, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, Var value, Iterator out) {
  if (holds_alternative<u32>(value)) {
    return WriteNat(context, get<u32>(value), out);
  } else {
    return Write(context, get<string_view>(value), out);
  }
}

template <typename Iterator>
Iterator Write(WriteContext& context, const VarList& values, Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Text& value, Iterator out) {
  return Write(context, value.text, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const TextList& values, Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ValueType& value, Iterator out) {
  return WriteFormat(context, value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ValueTypeList& values,
               Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ValueTypeList& values,
               string_view name,
               Iterator out) {
  if (!values.empty()) {
    out = WriteLpar(context, name, out);
    out = Write(context, values, out);
    out = WriteRpar(context, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const FunctionType& value, Iterator out) {
  out = Write(context, value.params, "param", out);
  out = Write(context, value.results, "result", out);
  return out;
}

template <typename Iterator>
Iterator WriteTypeUse(WriteContext& context,
                      const OptAt<Var>& value,
                      Iterator out) {
  if (value) {
    out = WriteLpar(context, "type", out);
    out = Write(context, value->value(), out);
    out = WriteRpar(context, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const FunctionTypeUse& value,
               Iterator out) {
  out = WriteTypeUse(context, value.type_use, out);
  out = Write(context, *value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const v128& value, Iterator out) {
  u32x4 immediate = value.as<u32x4>();
  out = Write(context, "i32x4"_sv, out);
  for (auto& lane : immediate) {
    out = Write(context, lane, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BlockImmediate& value,
               Iterator out) {
  out = Write(context, value.label, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BrOnExnImmediate& value,
               Iterator out) {
  out = Write(context, *value.target, out);
  out = Write(context, *value.event, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BrTableImmediate& value,
               Iterator out) {
  out = Write(context, value.targets, out);
  out = Write(context, *value.default_target, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const CallIndirectImmediate& value,
               Iterator out) {
  out = Write(context, value.table, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const CopyImmediate& value,
               Iterator out) {
  out = Write(context, value.dst, out);
  out = Write(context, value.src, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const InitImmediate& value,
               Iterator out) {
  // Write dcontext, out, st first, if it exists.
  if (value.dst) {
    out = Write(context, value.dst->value(), out);
  }
  out = Write(context, *value.segment, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const MemArgImmediate& value,
               Iterator out) {
  if (value.offset) {
    out = Write(context, "offset="_sv, out);
    context.ClearSeparator();
    out = Write(context, value.offset->value(), out);
  }

  if (value.align) {
    out = Write(context, "align="_sv, out);
    context.ClearSeparator();
    out = Write(context, value.align->value(), out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ShuffleImmediate& value,
               Iterator out) {
  return WriteRange(context, value.begin(), value.end(), out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Opcode& value, Iterator out) {
  return WriteFormat(context, value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Instruction& value, Iterator out) {
  out = Write(context, *value.opcode, out);

  switch (value.immediate.index()) {
    case 0: // monostate
      break;

    case 1: // s32
      out = Write(context, get<At<s32>>(value.immediate), out);
      break;

    case 2: // s64
      out = Write(context, get<At<s64>>(value.immediate), out);
      break;

    case 3: // f32
      out = Write(context, get<At<f32>>(value.immediate), out);
      break;

    case 4: // f64
      out = Write(context, get<At<f64>>(value.immediate), out);
      break;

    case 5: // v128
      out = Write(context, get<At<v128>>(value.immediate), out);
      break;

    case 6: // Var
      out = Write(context, get<At<Var>>(value.immediate), out);
      break;

    case 7: // BlockImmediate
      out = Write(context, get<At<BlockImmediate>>(value.immediate), out);
      break;

    case 8: // BrOnExnImmediate
      out = Write(context, get<At<BrOnExnImmediate>>(value.immediate), out);
      break;

    case 9: // BrTableImmediate
      out = Write(context, get<At<BrTableImmediate>>(value.immediate), out);
      break;

    case 10: // CallIndirectImmediate
      out =
          Write(context, get<At<CallIndirectImmediate>>(value.immediate), out);
      break;

    case 11: // CopyImmediate
      out = Write(context, get<At<CopyImmediate>>(value.immediate), out);
      break;

    case 12: // InitImmediate
      out = Write(context, get<At<InitImmediate>>(value.immediate), out);
      break;

    case 13: // MemArgImmediate
      out = Write(context, get<At<MemArgImmediate>>(value.immediate), out);
      break;

    case 14: // ReferenceType
      out = Write(context, get<At<ReferenceType>>(value.immediate), out);
      break;

    case 15: // SelectImmediate
      out = Write(context, get<At<SelectImmediate>>(value.immediate), out);
      break;

    case 16: // ShuffleImmediate
      out = Write(context, get<At<ShuffleImmediate>>(value.immediate), out);
      break;

    case 17: // SimdLaneImmediate
      out = Write(context, get<At<SimdLaneImmediate>>(value.immediate), out);
      break;
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const InstructionList& values,
               Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator WriteWithNewlines(WriteContext& context,
                           const InstructionList& values,
                           Iterator out) {
  for (auto& value : values) {
    auto opcode = value->opcode;
    if (opcode == Opcode::End || opcode == Opcode::Else ||
        opcode == Opcode::Catch) {
      context.Dedent();
      context.Newline();
    }

    out = Write(context, value, out);

    if (holds_alternative<At<BlockImmediate>>(value->immediate) ||
        opcode == Opcode::Else || opcode == Opcode::Catch) {
      context.Indent();
    }
    context.Newline();
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BoundValueType& value,
               Iterator out) {
  out = Write(context, value.name, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BoundValueTypeList& values,
               string_view prefix,
               Iterator out) {
  bool first = true;
  bool prev_has_name = false;
  for (auto& value : values) {
    bool has_name = value->name.has_value();
    if ((has_name || prev_has_name) && !first) {
      out = WriteRpar(context, out);
    }
    if (has_name || prev_has_name || first) {
      out = WriteLpar(context, prefix, out);
    }
    if (has_name) {
      out = Write(context, value->name, out);
    }
    out = Write(context, value->type, out);
    prev_has_name = has_name;
    first = false;
  }
  if (!values.empty()) {
    out = WriteRpar(context, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const BoundFunctionType& value,
               Iterator out) {
  out = Write(context, value.params, "param", out);
  out = Write(context, value.results, "result", out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const TypeEntry& value, Iterator out) {
  out = WriteLpar(context, "type", out);
  out = WriteLpar(context, "func", out);
  out = Write(context, value.bind_var, out);
  out = Write(context, value.type, out);
  out = WriteRpar(context, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const FunctionDesc& value, Iterator out) {
  out = Write(context, "func"_sv, out);
  out = Write(context, value.name, out);
  out = WriteTypeUse(context, value.type_use, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Limits& value, Iterator out) {
  out = Write(context, value.min, out);
  if (value.max) {
    out = Write(context, *value.max, out);
  }
  if (value.shared == Shared::Yes) {
    out = Write(context, "shared"_sv, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, ReferenceType value, Iterator out) {
  return WriteFormat(context, value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const TableType& value, Iterator out) {
  out = Write(context, value.limits, out);
  out = WriteFormat(context, value.elemtype, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const TableDesc& value, Iterator out) {
  out = Write(context, "table"_sv, out);
  out = Write(context, value.name, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const MemoryType& value, Iterator out) {
  out = Write(context, value.limits, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const MemoryDesc& value, Iterator out) {
  out = Write(context, "memory"_sv, out);
  out = Write(context, value.name, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const GlobalType& value, Iterator out) {
  if (value.mut == Mutability::Var) {
    out = WriteLpar(context, "mut", out);
  }
  out = Write(context, value.valtype, out);
  if (value.mut == Mutability::Var) {
    out = WriteRpar(context, out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const GlobalDesc& value, Iterator out) {
  out = Write(context, "global"_sv, out);
  out = Write(context, value.name, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const EventType& value, Iterator out) {
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const EventDesc& value, Iterator out) {
  out = Write(context, "event"_sv, out);
  out = Write(context, value.name, out);
  out = Write(context, value.type, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Import& value, Iterator out) {
  out = WriteLpar(context, "import"_sv, out);
  out = Write(context, value.module, out);
  out = Write(context, value.name, out);
  out = WriteLpar(context, out);
  switch (value.desc.index()) {
    case 0:  // FunctionDesc
      out = Write(context, get<FunctionDesc>(value.desc), out);
      break;

    case 1:  // TableDesc
      out = Write(context, get<TableDesc>(value.desc), out);
      break;

    case 2:  // MemoryDesc
      out = Write(context, get<MemoryDesc>(value.desc), out);
      break;

    case 3:  // GlobalDesc
      out = Write(context, get<GlobalDesc>(value.desc), out);
      break;

    case 4:  // EventDesc
      out = Write(context, get<EventDesc>(value.desc), out);
      break;
  }
  out = WriteRpar(context, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const InlineImport& value, Iterator out) {
  out = WriteLpar(context, "import"_sv, out);
  out = Write(context, value.module, out);
  out = Write(context, value.name, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const InlineExport& value, Iterator out) {
  out = WriteLpar(context, "export"_sv, out);
  out = Write(context, value.name, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const InlineExportList& values,
               Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Function& value, Iterator out) {
  out = WriteLpar(context, "func", out);

  // Can't write FunctionDesc directly, since inline imports/exports occur
  // between the bindvar and the type use.
  out = Write(context, value.desc.name, out);
  out = Write(context, value.exports, out);

  if (value.import) {
    out = Write(context, *value.import, out);
  }

  out = WriteTypeUse(context, value.desc.type_use, out);
  out = Write(context, value.desc.type, out);

  if (!value.import) {
    context.Indent();
    context.Newline();
    out = Write(context, value.locals, "local", out);
    context.Newline();
    out = WriteWithNewlines(context, value.instructions, out);
    context.Dedent();
  }

  out = WriteRpar(context, out);
  context.Newline();
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ElementExpressionList& elem_exprs,
               Iterator out) {
  // Use spaces instead of newlines for element expressions.
  for (auto& elem_expr : elem_exprs) {
    for (auto& instr : elem_expr->instructions) {
      // Expressions need to be wrapped in parens.
      out = WriteLpar(context, out);
      out = Write(context, instr, out);
      out = WriteRpar(context, out);
      context.Space();
    }
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ElementListWithExpressions& value,
               Iterator out) {
  out = Write(context, value.elemtype, out);
  out = Write(context, value.list, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, ExternalKind value, Iterator out) {
  return WriteFormat(context, value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ElementListWithVars& value,
               Iterator out) {
  out = Write(context, value.kind, out);
  out = Write(context, value.list, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ElementList& value, Iterator out) {
  if (holds_alternative<ElementListWithVars>(value)) {
    return Write(context, get<ElementListWithVars>(value), out);
  } else {
    return Write(context, get<ElementListWithExpressions>(value), out);
  }
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Table& value, Iterator out) {
  out = WriteLpar(context, "table", out);

  // Can't write TableDesc directly, since inline imports/exports occur after
  // the bind var.
  out = Write(context, value.desc.name, out);
  out = Write(context, value.exports, out);

  if (value.import) {
    out = Write(context, *value.import, out);
    out = Write(context, value.desc.type, out);
  } else if (value.elements) {
    // Don't write the limits, because they are implicitly defined by the
    // element segment length.
    out = Write(context, value.desc.type->elemtype, out);
    out = WriteLpar(context, "elem", out);
    // Only write the list of elements, without the ExternalKind or
    // ReferenceType.
    if (holds_alternative<ElementListWithVars>(*value.elements)) {
      out = Write(context, get<ElementListWithVars>(*value.elements).list, out);
    } else {
      out = Write(context,
                  get<ElementListWithExpressions>(*value.elements).list, out);
    }
    out = WriteRpar(context, out);
  } else {
    out = Write(context, value.desc.type, out);
  }

  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Memory& value, Iterator out) {
  out = WriteLpar(context, "memory", out);

  // Can't write MemoryDesc directly, since inline imports/exports occur after
  // the bind var.
  out = Write(context, value.desc.name, out);
  out = Write(context, value.exports, out);

  if (value.import) {
    out = Write(context, *value.import, out);
    out = Write(context, value.desc.type, out);
  } else if (value.data) {
    out = WriteLpar(context, "data", out);
    out = Write(context, value.data, out);
    out = WriteRpar(context, out);
  } else {
    out = Write(context, value.desc.type, out);
  }

  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ConstantExpression& value,
               Iterator out) {
  return Write(context, value.instructions, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Global& value, Iterator out) {
  out = WriteLpar(context, "global", out);

  // Can't write GlobalDesc directly, since inline imports/exports occur after
  // the bind var.
  out = Write(context, value.desc.name, out);
  out = Write(context, value.exports, out);

  if (value.import) {
    out = Write(context, *value.import, out);
    out = Write(context, value.desc.type, out);
  } else {
    out = Write(context, value.desc.type, out);
    out = Write(context, value.init, out);
  }

  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Export& value, Iterator out) {
  out = WriteLpar(context, "export", out);
  out = Write(context, value.name, out);
  out = WriteLpar(context, out);
  out = Write(context, value.kind, out);
  out = Write(context, value.var, out);
  out = WriteRpar(context, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Start& value, Iterator out) {
  out = WriteLpar(context, "start", out);
  out = Write(context, value.var, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ElementExpression& value,
               Iterator out) {
  return Write(context, value.instructions, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ElementSegment& value,
               Iterator out) {
  out = WriteLpar(context, "elem", out);
  out = Write(context, value.name, out);
  switch (value.type) {
    case SegmentType::Active:
      if (value.table) {
        out = WriteLpar(context, "table", out);
        out = Write(context, *value.table, out);
        out = WriteRpar(context, out);
      }
      if (value.offset) {
        out = WriteLpar(context, "offset", out);
        out = Write(context, *value.offset, out);
        out = WriteRpar(context, out);
      }

      // When writing a function var list, we can omit the "func" keyword to
      // remain compatible with the MVP text format.
      if (holds_alternative<ElementListWithVars>(value.elements)) {
        auto& element_vars = get<ElementListWithVars>(value.elements);
        //  The legacy format which omits the external kind cannot be used with
        //  the "table use" or bind_var syntax.
        if (element_vars.kind != ExternalKind::Function || value.table ||
            value.name) {
          out = Write(context, element_vars.kind, out);
        }
        out = Write(context, element_vars.list, out);
      } else {
        out = Write(context,
                    get<ElementListWithExpressions>(value.elements), out);
      }
      break;

    case SegmentType::Passive:
      out = Write(context, value.elements, out);
      break;

    case SegmentType::Declared:
      out = Write(context, "declare"_sv, out);
      out = Write(context, value.elements, out);
      break;
  }
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const DataSegment& value, Iterator out) {
  out = WriteLpar(context, "data", out);
  out = Write(context, value.name, out);
  if (value.type == SegmentType::Active) {
    if (value.memory) {
      out = WriteLpar(context, "memory", out);
      out = Write(context, *value.memory, out);
      out = WriteRpar(context, out);
    }
    if (value.offset) {
      out = WriteLpar(context, "offset", out);
      out = Write(context, *value.offset, out);
      out = WriteRpar(context, out);
    }
  }

  out = Write(context, value.data, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Event& value, Iterator out) {
  out = WriteLpar(context, "event", out);

  // Can't write EventDesc directly, since inline imports/exports occur after
  // the bind var.
  out = Write(context, value.desc.name, out);
  out = Write(context, value.exports, out);

  if (value.import) {
    out = Write(context, *value.import, out);
    out = Write(context, value.desc.type, out);
  } else {
    out = Write(context, value.desc.type, out);
  }

  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ModuleItem& value, Iterator out) {
  switch (value.index()) {
    case 0: // TypeEntry
      out = Write(context, get<At<TypeEntry>>(value), out);
      break;

    case 1: // Import
      out = Write(context, get<At<Import>>(value), out);
      break;

    case 2:  // Function
      out = Write(context, get<At<Function>>(value), out);
      break;

    case 3:  // Table
      out = Write(context, get<At<Table>>(value), out);
      break;

    case 4:  // Memory
      out = Write(context, get<At<Memory>>(value), out);
      break;

    case 5:  // Global
      out = Write(context, get<At<Global>>(value), out);
      break;

    case 6:  // Export
      out = Write(context, get<At<Export>>(value), out);
      break;

    case 7:  // Start
      out = Write(context, get<At<Start>>(value), out);
      break;

    case 8:  // ElementSegment
      out = Write(context, get<At<ElementSegment>>(value), out);
      break;

    case 9:  // DataSegment
      out = Write(context, get<At<DataSegment>>(value), out);
      break;

    case 10:  // Event
      out = Write(context, get<At<Event>>(value), out);
      break;
  }
  context.Newline();
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Module& value, Iterator out) {
  return WriteVector(context, value, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ScriptModule& value, Iterator out) {
  out = WriteLpar(context, "module", out);
  out = Write(context, value.name, out);
  switch (value.kind) {
    case ScriptModuleKind::Text:
      context.Indent();
      context.Newline();
      out = Write(context, get<Module>(value.module), out);
      context.Dedent();
      break;

    case ScriptModuleKind::Binary:
      out = Write(context, "binary"_sv, out);
      out = Write(context, get<TextList>(value.module), out);
      break;

    case ScriptModuleKind::Quote:
      out = Write(context, "quote"_sv, out);
      out = Write(context, get<TextList>(value.module), out);
      break;
  }
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Const& value, Iterator out) {
  out = WriteLpar(context, out);
  switch (value.index()) {
    case 0: // u32
      out = Write(context, Opcode::I32Const, out);
      out = Write(context, get<u32>(value), out);
      break;

    case 1: // u64
      out = Write(context, Opcode::I64Const, out);
      out = Write(context, get<u64>(value), out);
      break;

    case 2: // f32
      out = Write(context, Opcode::F32Const, out);
      out = Write(context, get<f32>(value), out);
      break;

    case 3: // f64
      out = Write(context, Opcode::F64Const, out);
      out = Write(context, get<f64>(value), out);
      break;

    case 4: // v128
      out = Write(context, Opcode::V128Const, out);
      out = Write(context, get<v128>(value), out);
      break;

    case 5: // RefNullConst
      out = Write(context, Opcode::RefNull, out);
      break;

    case 6: // RefExternConst
      out = Write(context, "ref.extern"_sv, out);
      out = Write(context, get<RefExternConst>(value).var, out);
      break;
  }
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ConstList& values, Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const InvokeAction& value, Iterator out) {
  out = WriteLpar(context, "invoke", out);
  out = Write(context, value.module, out);
  out = Write(context, value.name, out);
  out = Write(context, value.consts, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const GetAction& value, Iterator out) {
  out = WriteLpar(context, "get", out);
  out = Write(context, value.module, out);
  out = Write(context, value.name, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Action& value, Iterator out) {
  if (holds_alternative<InvokeAction>(value)) {
    out = Write(context, get<InvokeAction>(value), out);
  } else if (holds_alternative<GetAction>(value)) {
    out = Write(context, get<GetAction>(value), out);
  }
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ModuleAssertion& value,
               Iterator out) {
  out = Write(context, value.module, out);
  context.Newline();
  out = Write(context, value.message, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ActionAssertion& value,
               Iterator out) {
  out = Write(context, value.action, out);
  out = Write(context, value.message, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const NanKind& value, Iterator out) {
  if (value == NanKind::Arithmetic) {
    return Write(context, "nan:arithmetic"_sv, out);
  } else {
    assert(value == NanKind::Canonical);
    return Write(context, "nan:canonical"_sv, out);
  }
}

template <typename Iterator, typename T>
Iterator Write(WriteContext& context,
               const FloatResult<T>& value,
               Iterator out) {
  if (holds_alternative<T>(value)) {
    return Write(context, get<T>(value), out);
  } else {
    return Write(context, get<NanKind>(value), out);
  }
}

template <typename Iterator, typename T, size_t N>
Iterator Write(WriteContext& context,
               const std::array<FloatResult<T>, N>& value,
               Iterator out) {
  return WriteRange(context, value.begin(), value.end(), out);
}

template <typename Iterator>
Iterator Write(WriteContext& context, const ReturnResult& value, Iterator out) {
  out = WriteLpar(context, out);
  switch (value.index()) {
    case 0: // u32
      out = Write(context, Opcode::I32Const, out);
      out = Write(context, get<u32>(value), out);
      break;

    case 1: // u64
      out = Write(context, Opcode::I64Const, out);
      out = Write(context, get<u64>(value), out);
      break;

    case 2: // v128
      out = Write(context, Opcode::V128Const, out);
      out = Write(context, get<v128>(value), out);
      break;

    case 3: // F32Result
      out = Write(context, Opcode::F32Const, out);
      out = Write(context, get<F32Result>(value), out);
      break;

    case 4: // F64Result
      out = Write(context, Opcode::F64Const, out);
      out = Write(context, get<F64Result>(value), out);
      break;

    case 5: // F32x4Result
      out = Write(context, Opcode::V128Const, out);
      out = Write(context, "f32x4"_sv, out);
      out = Write(context, get<F32x4Result>(value), out);
      break;

    case 6: // F64x2Result
      out = Write(context, Opcode::V128Const, out);
      out = Write(context, "f64x2"_sv, out);
      out = Write(context, get<F64x2Result>(value), out);
      break;

    case 7: // RefNullConst
      out = Write(context, Opcode::RefNull, out);
      break;

    case 8: // RefExternConst
      out = Write(context, "ref.extern"_sv, out);
      out = WriteNat(context, *get<RefExternConst>(value).var, out);
      break;

    case 9: // RefExternResult
      out = Write(context, "ref.extern"_sv, out);
      break;

    case 10: // RefFuncResult
      out = Write(context, "ref.func"_sv, out);
      break;
  }
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ReturnResultList& values,
               Iterator out) {
  return WriteVector(context, values, out);
}

template <typename Iterator>
Iterator Write(WriteContext& context,
               const ReturnAssertion& value,
               Iterator out) {
  out = Write(context, value.action, out);
  out = Write(context, value.results, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Assertion& value, Iterator out) {
  switch (value.kind) {
    case AssertionKind::Malformed:
      out = WriteLpar(context, "assert_malformed", out);
      context.Indent();
      context.Newline();
      out = Write(context, get<ModuleAssertion>(value.desc), out);
      context.Dedent();
      break;

    case AssertionKind::Invalid:
      out = WriteLpar(context, "assert_invalid", out);
      context.Indent();
      context.Newline();
      out = Write(context, get<ModuleAssertion>(value.desc), out);
      context.Dedent();
      break;

    case AssertionKind::Unlinkable:
      out = WriteLpar(context, "assert_unlinkable", out);
      context.Indent();
      context.Newline();
      out = Write(context, get<ModuleAssertion>(value.desc), out);
      context.Dedent();
      break;

    case AssertionKind::ActionTrap:
      out = WriteLpar(context, "assert_trap", out);
      out = Write(context, get<ActionAssertion>(value.desc), out);
      break;

    case AssertionKind::Return:
      out = WriteLpar(context, "assert_return", out);
      out = Write(context, get<ReturnAssertion>(value.desc), out);
      break;

    case AssertionKind::ModuleTrap:
      out = WriteLpar(context, "assert_trap", out);
      context.Indent();
      context.Newline();
      out = Write(context, get<ModuleAssertion>(value.desc), out);
      context.Dedent();
      break;

    case AssertionKind::Exhaustion:
      out = WriteLpar(context, "assert_exhaustion", out);
      out = Write(context, get<ActionAssertion>(value.desc), out);
      break;
  }
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Register& value, Iterator out) {
  out = WriteLpar(context, "register", out);
  out = Write(context, value.name, out);
  out = Write(context, value.module, out);
  out = WriteRpar(context, out);
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Command& value, Iterator out) {
  switch (value.index()) {
    case 0:  // ScriptModule
      out = Write(context, get<ScriptModule>(value), out);
      break;

    case 1:  // Register
      out = Write(context, get<Register>(value), out);
      break;

    case 2:  // Action
      out = Write(context, get<Action>(value), out);
      break;

    case 3:  // Assertion
      out = Write(context, get<Assertion>(value), out);
      break;
  }
  context.Newline();
  return out;
}

template <typename Iterator>
Iterator Write(WriteContext& context, const Script& values, Iterator out) {
  return WriteVector(context, values, out);
}

}  // namespace text
}  // namespace wasp

#endif  // WASP_TEXT_WRITE_H_
