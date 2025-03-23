# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

#!/usr/bin/env python3
import os, sys
import glob
import importlib

resource_dict = {}
operation_dict = {}

def generate_doc(item):
  doc = {}
  if "doc" in item:
    doc = item["doc"]
  in_group = doc.get("in_group", "LCI_OTHER")
  params = []
  returns = []

  if item["category"] == "operation":
    # For operation 
    group_id = f"LCI_{item['name'].upper()}"
    group_name = f"{item['name']}"
    brief = doc.get("brief", f"LCI operation {item['name']}")
    details = doc.get("details", "")

    arguments = [arg for arg in item["args"] if arg["category"] == "argument"]
    return_vals = [arg for arg in item["args"] if arg["category"] == "return_val"]

    for arg in arguments:
      inout_trait = ""
      if "in" in arg["trait"]:
        inout_trait = "[in]"
      elif "out" in arg["trait"]:
        inout_trait = "[out]"
      elif "inout" in arg["trait"]:
        inout_trait = "[in,out]"
      
      pos_opt_trait = ""
      if "positional" in arg["trait"]:
        pos_opt_trait = "positional"
      elif "optional" in arg["trait"]:
        pos_opt_trait = "optional"
      params.append(f" * @param{inout_trait} {arg['name']} of type __{arg['type']}__; {pos_opt_trait} argument; {arg['comment']}")

    for return_val in return_vals:
      returns.append(f" * @return of type __{return_val['type']}__; {return_val['comment']}")
  
  elif item["category"] == "resource":  
    # For resource
    group_id = f"LCI_{item['name'].upper()}"
    group_name = f"RESOURCE {item['name']}"
    brief = doc.get("brief", f"LCI resource {item['name']}")
    details = doc.get("details", "")

    for attr in item["attrs"]:
      inout_trait = ""
      if "in" in attr["trait"]:
        inout_trait = "[in]"
      elif "out" in attr["trait"]:
        inout_trait = "[out]"
      elif "inout" in attr["trait"]:
        inout_trait = "[in,out]"
      params.append(f" * @param{inout_trait} {attr['name']} of type __{attr['type']}__; {attr['comment']}")
  params = "\n".join(params)
  returns = "\n".join(returns)

  return f"""
/**
 * @defgroup {group_id} {group_name}
 * @ingroup {in_group}
 * @brief {brief}
 * @details {details}
{params}
{returns}
 */
 
 /**
  * @ingroup {group_id}
  * @brief The actual implementation for {group_name}.
  */"""

def generate_forward_resource_decl(input):
  text = ""
  for item in input:
    if item["category"] == "resource":
      resource_name = item["name"]
      text += f"class {resource_name}_t;\n"
      text += f"class {resource_name}_attr_t;\n"
  text += "\n"
  return text

def generate_resource_attr_decl(item):
  assert item["category"] == "resource"
  resource_name = item["name"]
  attrs = item["attrs"]

  # build attributes list
  attr_list = ""
  for attr in attrs:
    attr_list += f"  {attr['type']} {attr['name']};\n"
  text = f"""
struct {resource_name}_attr_t {{
{attr_list}}};
"""
  return text

def generate_enum_attr_class_decl(attr):
  enum_delcaration = ""
  for item in input:
    if item["category"] == "resource":
      for attr in item["attrs"]:
        # declare enum class
        if "enum" not in attr["trait"]:
          continue
        type_name = attr["type"]
        enum_delcaration += f"enum class {type_name} {{\n"
        for option in attr["enum_options"]:
          enum_delcaration += f"  {option},\n"
        enum_delcaration += f"}};\n\n"
  return enum_delcaration

def generate_global_attr_decl(input):
  # declare global attribute struct
  variable_decl = "struct global_attr_t {\n"
  for item in input:
    if item["category"] == "resource":
      for attr in item["attrs"]:
        if "not_global" in attr["trait"]:
          # user context is not global
          continue
        if "out" in attr["trait"]:
          # output attribute does not have global default value
          continue
        type_name = attr["type"]
        var_name = attr["name"]
        # declare enum class
        variable_decl += f"  {type_name} {var_name};\n"
  variable_decl += "};\n"
  return variable_decl


def generate_global_attr_impl(input):
  # implement global attribute struct
  text = ""
  for item in input:
    if item["category"] == "resource":
      for attr in item["attrs"]:
        if "out" in attr["trait"]:
          # output attribute does not have global default value
          continue
        if "not_global" in attr["trait"]:
          continue
        if "default_value" in attr and attr["default_value"] is not None:
          type_name = attr["type"]
          env_var = f"LCI_ATTR_{attr['name'].upper()}"
          attr_name = attr['name']
          if "enum" in attr["trait"]:
            default_value = attr["default_value"]
            dict_decl = ""
            for option in attr["enum_options"]:
              dict_decl += f"         {{\"{option}\", static_cast<int>({type_name}::{option})}},\n"
            text += f"""
  {{
    // default value
    g_default_attr.{attr_name} = {type_name}::{default_value};
    // if users explicitly set the value
    char* p = getenv("{env_var}");
    if (p) {{
      LCT_dict_str_int_t dict[] = {{
{dict_decl}
      }};
      g_default_attr.{attr_name} =
          static_cast<{type_name}>(LCT_parse_arg(dict, sizeof(dict) / sizeof(dict[0]), p, ","));
    }}
    LCI_Log(LOG_INFO, "env", "set {attr_name} to be %d\\n",
              static_cast<int>(g_default_attr.{attr_name}));
  }}\n"""
          else:
            text += f"  g_default_attr.{attr_name} = get_env_or(\"{env_var}\", {attr['default_value']});\n"
            if type_name == "std::string":
              text += f"  LCI_Log(LOG_INFO, \"env\", \"set {attr_name} to be %s\\n\", g_default_attr.{attr_name}.c_str());\n"
            else:
              text += f"  LCI_Log(LOG_INFO, \"env\", \"set {attr_name} to be %d\\n\", static_cast<int>(g_default_attr.{attr_name}));\n"
  
  code = f"""  
global_attr_t g_default_attr;

void init_global_attr() {{
{text}}}
"""
  return code
  

def generate_resource_decl(item):
  assert item["category"] == "resource"
  resource_name = item["name"]
  impl_class_name = resource_name + "_impl_t"
  attrs = item["attrs"]

  # build attribute getter
  attr_getter = ""
  for attr in attrs:
    attr_getter += f"  {attr['type']} get_attr_{attr['name']}() const;\n"
  if len(attrs) > 0:
    attr_getter += "  attr_t get_attr() const;"

  text = f"""
class {impl_class_name};
{generate_doc(item)}
class {resource_name}_t {{
 public:
  using attr_t = {resource_name}_attr_t;
  // attribute getter
{attr_getter}
  {impl_class_name}* p_impl = nullptr;

  {resource_name}_t() = default;
  {resource_name}_t(void* p) : p_impl(static_cast<{impl_class_name}*>(p)) {{}}
  inline bool is_empty() const {{ return p_impl == nullptr; }}
  inline {impl_class_name} *get_impl() const {{ if (!p_impl) throw std::runtime_error("{resource_name} p_impl is nullptr!"); return p_impl; }}
  inline void set_impl({impl_class_name}* p) {{ p_impl = p; }}
  bool operator==(const {resource_name}_t& other) const {{ return p_impl == other.p_impl; }}
}};
"""
  return text

def generate_resource_impl(item):
  assert item["category"] == "resource"
  class_name = item["name"] + "_t"
  attrs = item["attrs"]

  getter_impl = ""
  for attr in attrs:
    getter_impl += f"{attr['type']} {class_name}::get_attr_{attr['name']}() const {{ return p_impl->attr.{attr['name']}; }}\n"
  if len(attrs) > 0:
    getter_impl += "\n{}::attr_t {}::get_attr() const {{ return p_impl->attr; }}\n".format(class_name, class_name)
  return getter_impl


def generate_operation_decl(item):
  assert item["category"] == "operation"
  operation_name = item["name"]
  class_name = operation_name + "_x"
  return_vals = [arg for arg in item["args"] if arg["category"] == "return_val"]
  args = [arg for arg in item["args"] if arg["category"] == "argument"]
  positional_args = [arg for arg in args if "positional" in arg["trait"]]
  optional_args = [arg for arg in args if "optional" in arg["trait"]]

  # build args declaration
  args_declaration = ""
  for arg in positional_args:
    args_declaration += f"  {arg['type']} m_{arg['name']};\n"
  for arg in optional_args:
    args_declaration += f"  option_t<{arg['type']}> m_{arg['name']} ;\n"
  
  # build constructor
  # positional_args_str = ", ".join([f"const {arg["type"]}& {arg["name"]}_in" for arg in positional_args])
  positional_args_str = ", ".join([f"{arg['type']} {arg['name']}_in" for arg in positional_args])

  # constructor impl
  positional_args_str = ", ".join([f"{arg['type']} {arg['name']}_in" for arg in positional_args])
  positional_args_name_str = ", ".join([f"{arg['name']}_in" for arg in positional_args])
  signature = f"{class_name}({positional_args_str})"
  init_list = [f"m_{arg['name']}({arg['name']}_in)" for arg in positional_args]
  # init_list += [f"m_{arg['name']}({arg['default_value']}, false)" for arg in optional_args]
  if len(init_list) > 0:
    init_list_str = ", ".join(init_list)
    constructor = f"  {signature} : {init_list_str} {{}}\n"
  else:
    constructor = f"  {signature} {{}}\n"
  # constructor = f"  {class_name}({positional_args_str});\n"
  
  # build setter
  setter = ""
  for arg in positional_args + optional_args:
    type_name = arg["type"]
    name = arg["name"]
    setter += f"  inline {class_name}&& {name}({type_name} {name}_in) {{ m_{name} = {name}_in; return std::move(*this); }}\n"
    # setter += f"  void {name}(const {type_name}& {name}_in) {{ {name}_ = {name}_in; }}\n"
    # setter += f"  void {name}({type_name}&& {name}_in) {{ {name}_ = std::move({name}_in); }}\n"

  if len(return_vals) > 1:
    type_str = ", ".join([f"{ret['type']}" for ret in return_vals])
    return_vals_str = f"std::tuple<{type_str}>"
  elif len(return_vals) == 1:
    return_vals_str = f"{return_vals[0]['type']}"
  else:
    return_vals_str = "void"

  call_impl_args_decl = ", ".join([f"{arg['type']} {arg['name']}" for arg in positional_args + optional_args])
  # call_impl_args = ", ".join([f"m_{arg['name']}" for arg in positional_args + optional_args])

  text = f"""
{generate_doc(item)}
class {class_name} {{
 public:
  // args declaration
{args_declaration}
  // constructor
{constructor}
  // setter
{setter}
  {return_vals_str} call_impl({call_impl_args_decl}) const;
  {return_vals_str} call() const;
  inline {return_vals_str} operator()() const {{ return call(); }}
}};

inline {return_vals_str} {operation_name}({positional_args_str}) {{
  return {class_name}({positional_args_name_str}).call();
}}
"""
  return text

def generate_operation_impl(item):
  assert item["category"] == "operation"
  class_name = item["name"] + "_x"
  return_vals = [arg for arg in item["args"] if arg["category"] == "return_val"]
  args = [arg for arg in item["args"] if arg["category"] == "argument"]
  positional_args = [arg for arg in args if "positional" in arg["trait"]]
  optional_args = [arg for arg in args if "optional" in arg["trait"]]

  # call wrapper
  if len(return_vals) > 1:
    type_str = ", ".join([f"{ret['type']}" for ret in return_vals])
    return_vals_str = f"std::tuple<{type_str}>"
  elif len(return_vals) == 1:
    return_vals_str = f"{return_vals[0]['type']}"
  else:
    return_vals_str = "void"
  
  call_wrapper_signature = f"{return_vals_str} {class_name}::call() const"
  body = ""
  if item["init_global"]:
    body += "  global_initialize();\n"
  for arg in positional_args:
    body += f"  auto {arg['name']} = m_{arg['name']};\n"
  for arg in optional_args:
    body += f"  auto {arg['name']} = m_{arg['name']}.get_value_or({arg['default_value']});\n"
  call_impl_args = ", ".join([f"{arg['name']}" for arg in positional_args + optional_args])
  if item["fina_global"]:
    if return_vals_str != "void":
      body += f"  auto ret = call_impl({call_impl_args});\n"
      body += "  global_finalize();\n"
      body += "  return ret;\n"
    else:
      body += f"  call_impl({call_impl_args});\n"
      body += "  global_finalize();\n"
  else:
    body += f"  return call_impl({call_impl_args});\n"

  call_wrapper_impl = f"""
{call_wrapper_signature} {{
{body}}}
"""
  return call_wrapper_impl

def generate_header_pre(input):
  signature = "LCI_BINDING_PRE_HPP_"
  main_body = ""
  main_body += generate_forward_resource_decl(input)
  main_body += generate_enum_attr_class_decl(input)
  main_body += generate_global_attr_decl(input)
  resources = [x for x in input if x["category"] == "resource"]
  for item in resources:
    main_body += generate_resource_attr_decl(item)
    main_body += generate_resource_decl(item)
  return f"""
// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

// clang-format off
// This file is generated by generate_binding.py
#ifndef {signature}
#define {signature}

namespace lci {{
{main_body}

}} // namespace lci

#endif // {signature}
"""

def generate_header_post(input):
  signature = "LCI_BINDING_POST_HPP_"
  main_body = ""
  operations = [x for x in input if x["category"] == "operation"]
  for item in operations:
    main_body += generate_operation_decl(item)
  return f"""
// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

// clang-format off
// This file is generated by generate_binding.py
#ifndef {signature}
#define {signature}

namespace lci {{
{main_body}

}} // namespace lci

#endif // {signature}
"""

def generate_impl(input):
  main_body = ""
  main_body += generate_global_attr_impl(input)
  for item in input:
    if item["category"] == "resource":
      main_body += generate_resource_impl(item)
    elif item["category"] == "operation":
      main_body += generate_operation_impl(item)

  template = """
// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

// clang-format off
// This file is generated by generate_binding.py

#include "lci_internal.hpp"

namespace lci {{
{}
}} // namespace lci
"""
  return template.format(main_body)

def build_dicts(input):
  for item in input:
    if item["category"] == "resource":
      resource_dict[item["name"]] = item
    elif item["category"] == "operation":
      operation_dict[item["name"]] = item

def generate_binding(input):
  if os.path.exists(output_dir) == False:
    os.mkdir(output_dir)

  build_dicts(input)

  header_path = output_dir + "/lci_binding_pre.hpp"
  with open(header_path, "w") as f:
    f.write(generate_header_pre(input))
    print(f"Generated binding header_pre in {os.path.abspath(f.name)}")

  header_path = output_dir + "/lci_binding_post.hpp"
  with open(header_path, "w") as f:
    f.write(generate_header_post(input))
    print(f"Generated binding header_post in {os.path.abspath(f.name)}")

  source_path = output_dir + "/binding.cpp"
  with open(source_path, "w") as f:
    f.write(generate_impl(input))
    print(f"Generated binding source in {os.path.abspath(f.name)}")

if __name__ == "__main__":
  dir_path = os.path.dirname(os.path.realpath(__file__))
  input_dir =  dir_path + "/input"
  output_dir = dir_path + "/generated"

  input = []
  sys.path.append(input_dir)
  for filename in glob.glob(os.path.join(input_dir, "*.py")):
    module_basename = os.path.basename(filename).replace(".py", "")
    # module_path = filename.replace("/", ".").replace("\\", ".").replace(".py", "")
    module_path = "input.{}".format(module_basename)
    module = importlib.import_module(module_path)
    if hasattr(module, "get_input"):
      print("Reading input from", filename)
      input += module.get_input()
  if len(input) == 0:
    print("No input found. Exiting.")
    exit(1)
  generate_binding(input)