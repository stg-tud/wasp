//
// Copyright 2019 WebAssembly Community Group participants
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

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "src/tools/argparser.h"
#include "wasp/base/enumerate.h"
#include "wasp/base/features.h"
#include "wasp/base/file.h"
#include "wasp/base/format.h"
#include "wasp/base/formatters.h"
#include "wasp/base/optional.h"
#include "wasp/base/string_view.h"
#include "wasp/binary/errors_nop.h"
#include "wasp/binary/formatters.h"
#include "wasp/binary/lazy_expression.h"
#include "wasp/binary/visitor.h"
#include "wasp/valid/begin_code.h"
#include "wasp/valid/context.h"
#include "wasp/valid/errors.h"
#include "wasp/valid/validate_data_count.h"
#include "wasp/valid/validate_data_segment.h"
#include "wasp/valid/validate_element_segment.h"
#include "wasp/valid/validate_export.h"
#include "wasp/valid/validate_function.h"
#include "wasp/valid/validate_global.h"
#include "wasp/valid/validate_import.h"
#include "wasp/valid/validate_instruction.h"
#include "wasp/valid/validate_locals.h"
#include "wasp/valid/validate_memory.h"
#include "wasp/valid/validate_start.h"
#include "wasp/valid/validate_table.h"
#include "wasp/valid/validate_type_entry.h"

namespace wasp {
namespace tools {
namespace validate {

using namespace ::wasp::binary;

// TODO: share w/ dump.cc
class ErrorsBasic : public Errors, public valid::Errors {
 public:
  explicit ErrorsBasic(SpanU8 data) : data{data} {}

  using binary::Errors::OnError;
  using valid::Errors::OnError;

 protected:
  void HandlePushContext(SpanU8 pos, string_view desc) override {}
  void HandlePushContext(string_view desc) override {}
  void HandlePopContext() override {}
  void HandleOnError(SpanU8 pos, string_view message) override {
    print("{:08x}: {}\n", pos.data() - data.data(), message);
  }
  void HandleOnError(string_view message) override { print("{}\n", message); }

  SpanU8 data;
};

struct Options {
  Features features;
};

struct Tool {
  explicit Tool(string_view filename, SpanU8 data, Options);

  bool Run();

  struct Visitor : visit::Visitor {
    explicit Visitor(Tool&);

    visit::Result OnSection(Section);
    visit::Result OnType(const TypeEntry&);
    visit::Result OnImport(const Import&);
    visit::Result OnFunction(const Function&);
    visit::Result OnTable(const Table&);
    visit::Result OnMemory(const Memory&);
    visit::Result OnGlobal(const Global&);
    visit::Result OnExport(const Export&);
    visit::Result OnStart(const Start&);
    visit::Result OnElement(const ElementSegment&);
    visit::Result OnDataCount(const DataCount&);
    visit::Result OnCode(const Code&);
    visit::Result OnData(const DataSegment&);

    visit::Result FailUnless(bool);

    Tool& tool;
    valid::Context& context;
    Features& features;
    ErrorsBasic& errors;

    optional<SectionId> last_section_id;
  };

  std::string filename;
  Options options;
  SpanU8 data;
  ErrorsBasic errors;
  LazyModule module;

  valid::Context context;
};

void PrintHelp(int);

int Main(span<string_view> args) {
  std::vector<string_view> filenames;
  Options options;
  options.features.EnableAll();

  ArgParser parser;
  parser.Add('h', "--help", []() { PrintHelp(0); })
      .Add([&](string_view arg) { filenames.push_back(arg); });
  parser.Parse(args);

  if (filenames.empty()) {
    print("No filenames given.\n");
    PrintHelp(1);
  }

  bool ok = true;

  for (auto filename : filenames) {
    auto optbuf = ReadFile(filename);
    if (!optbuf) {
      print("Error reading file {}.\n", filename);
      ok = false;
      continue;
    }

    SpanU8 data{*optbuf};
    Tool tool{filename, data, options};
    ok &= tool.Run();
  }

  return ok ? 0 : 1;
}

void PrintHelp(int errcode) {
  print("usage: wasp validate [options] <filename.wasm>...\n");
  print("\n");
  print("options:\n");
  print(" -h  --help    print help and exit\n");
  exit(errcode);
}

Tool::Tool(string_view filename, SpanU8 data, Options options)
    : filename(filename),
      options{options},
      data{data},
      errors{data},
      module{ReadModule(data, options.features, errors)} {}

bool Tool::Run() {
  if (!(module.magic && module.version)) {
    return false;
  }

  Visitor visitor{*this};
  return visit::Visit(module, visitor) == visit::Result::Ok;
}

Tool::Visitor::Visitor(Tool& tool)
    : tool{tool},
      context{tool.context},
      features{tool.options.features},
      errors{tool.errors} {}

visit::Result Tool::Visitor::OnSection(Section section) {
  // TODO: Move into binary reading.
  static constexpr std::array<SectionId, 12> kSectionOrder = {{
      SectionId::Type,
      SectionId::Import,
      SectionId::Function,
      SectionId::Table,
      SectionId::Memory,
      SectionId::Global,
      SectionId::Export,
      SectionId::Start,
      SectionId::Element,
      SectionId::DataCount,
      SectionId::Code,
      SectionId::Data,
  }};

  auto get_order = [&](SectionId id) -> optional<int> {
    auto iter = std::find(kSectionOrder.begin(), kSectionOrder.end(), id);
    if (iter == kSectionOrder.end()) {
      errors.OnError(section.data(), format("Unknown section id: {}\n", id));
      return nullopt;
    }
    return iter - kSectionOrder.begin();
  };

  if (section.is_known()) {
    auto id = section.known().id;
    auto order = get_order(id);
    if (last_section_id && get_order(*last_section_id) >= order) {
      errors.OnError(section.data(),
                     format("Section out of order: {} cannot occur after {}\n",
                            id, *last_section_id));
      return visit::Result::Fail;
    }
    last_section_id = id;
  }

  return visit::Result::Ok;
}

visit::Result Tool::Visitor::OnType(const TypeEntry& type_entry) {
  return FailUnless(Validate(type_entry, context, features, errors));
}

visit::Result Tool::Visitor::OnImport(const Import& import) {
  return FailUnless(Validate(import, context, features, errors));
}

visit::Result Tool::Visitor::OnFunction(const Function& function) {
  return FailUnless(Validate(function, context, features, errors));
}

visit::Result Tool::Visitor::OnTable(const Table& table) {
  return FailUnless(Validate(table, context, features, errors));
}

visit::Result Tool::Visitor::OnMemory(const Memory& memory) {
  return FailUnless(Validate(memory, context, features, errors));
}

visit::Result Tool::Visitor::OnGlobal(const Global& global) {
  return FailUnless(Validate(global, context, features, errors));
}

visit::Result Tool::Visitor::OnExport(const Export& export_) {
  return FailUnless(Validate(export_, context, features, errors));
}

visit::Result Tool::Visitor::OnStart(const Start& start) {
  return FailUnless(Validate(start, context, features, errors));
}

visit::Result Tool::Visitor::OnElement(const ElementSegment& segment) {
  return FailUnless(Validate(segment, context, features, errors));
}

visit::Result Tool::Visitor::OnDataCount(const DataCount& data_count) {
  return FailUnless(Validate(data_count, context, features, errors));
}

visit::Result Tool::Visitor::OnCode(const Code& code) {
  if (!BeginCode(context, features, errors)) {
    return visit::Result::Fail;
  }

  for (const auto& locals : code.locals) {
    if (!Validate(locals, context, features, errors)) {
      return visit::Result::Fail;
    }
  }

  for (const auto& instruction :
       ReadExpression(code.body, features, errors)) {
    if (!Validate(instruction, context, features, errors)) {
      return visit::Result::Fail;
    }
  }

  return visit::Result::Ok;
}

visit::Result Tool::Visitor::OnData(const DataSegment& segment) {
  return FailUnless(Validate(segment, context, features, errors));
}

visit::Result Tool::Visitor::FailUnless(bool b) {
  return b ? visit::Result::Ok : visit::Result::Fail;
}

}  // namespace validate
}  // namespace tools
}  // namespace wasp
