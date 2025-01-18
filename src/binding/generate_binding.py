#!/usr/bin/env python3
import os, sys
import glob
import importlib

def generate_forward_resource_decl(input):
  text = ""
  for item in input:
    if item["type"] == "resource":
      resource_name = item["name"]
      text += f"class {resource_name}_t;\n"
      text += f"class {resource_name}_attr_t;\n"
  return text

def generate_attr_decl(input):
  text = ""

  # declare each resource's attribute struct
  for item in input:
    if not item["type"] == "resource":
      continue
    resource_name = item["name"]
    attrs = item["attrs"]

    # build attributes list
    attr_list = ""
    for attr in attrs:
      if attr[2] is not None:
        # default variable is provided
        attr_list += f"  {attr[0]} {attr[1]} = {attr[2]};\n"
      else:
        attr_list += f"  {attr[0]} {attr[1]};\n"

    text += f"""
struct {resource_name}_attr_t {{
{attr_list}}};
"""
    
  # declare global attribute struct
  text += "struct global_attr_t {\n"
  for item in input:
    if item["type"] == "resource":
      # if item["name"] == "runtime":
        # continue
      resource_name = item["name"]
      text += f"  {resource_name}_attr_t {resource_name}_attr;\n"
  text += "};\n"
  return text
  

def generate_resource_decl(item):
  assert item["type"] == "resource"
  resource_name = item["name"]
  impl_class_name = resource_name + "_impl_t"
  attrs = item["attrs"]

  # build attribute getter
  attr_getter = ""
  for attr in attrs:
    attr_getter += f"  {attr[0]} get_attr_{attr[1]}() const;\n"

  text = f"""
class {impl_class_name};
class {resource_name}_t {{
 public:
  using attr_t = {resource_name}_attr_t;
  // attribute getter
  attr_t get_attr() const;
{attr_getter}
  {impl_class_name}* p_impl = nullptr;
}};
"""
  return text

def generate_resource_impl(item):
  assert item["type"] == "resource"
  class_name = item["name"] + "_t"
  attrs = item["attrs"]

  getter_impl = "\n{}::attr_t {}::get_attr() const {{ return p_impl->attr; }}\n".format(class_name, class_name)
  for attr in attrs:
    getter_impl += f"{attr[0]} {class_name}::get_attr_{attr[1]}() const {{ return p_impl->attr.{attr[1]}; }}\n"
  return getter_impl


def generate_operation(item):
  assert item["type"] == "operation"
  class_name = item["name"] + "_x"
  all_args = item["args"]
  positional_args = [arg for arg in all_args if arg[2]] # is_positional

  # build args declaration
  args_declaration = ""
  for type_name, name, is_positional in all_args:
    if not is_positional:
      type_name = f"option_t<{type_name}>"
    args_declaration += f"  {type_name} {name}_;\n"
  
  # build constructor
  # positional_args_str = ", ".join([f"const {arg[0]}& {arg[1]}_in" for arg in positional_args])
  positional_args_str = ", ".join([f"{arg[0]} {arg[1]}_in" for arg in positional_args])
  signature = f"{class_name}({positional_args_str})"
  if len(positional_args) > 0:
    init_list = ", ".join([f"{arg[1]}_({arg[1]}_in)" for arg in positional_args])
    constructor = f"  {signature} : {init_list} {{}}\n"
  else:
    constructor = f"  {signature} {{}}\n"
  
  # build setter
  setter = ""
  for type_name, name, is_positional in all_args:
    setter += f"  {class_name}&& {name}({type_name} {name}_in) {{ {name}_ = {name}_in; return std::move(*this); }}\n"
    # setter += f"  void {name}(const {type_name}& {name}_in) {{ {name}_ = {name}_in; }}\n"
    # setter += f"  void {name}({type_name}&& {name}_in) {{ {name}_ = std::move({name}_in); }}\n"

  template = """
class {} {{
 public:
  // args declaration
{}
  // constructor
{}
  // setter
{}
  void call() const;
}};
"""
  return template.format(class_name, args_declaration, constructor, setter)

def generate_header(input):
  main_body = ""
  main_body += generate_forward_resource_decl(input)
  main_body += generate_attr_decl(input)
  for item in input:
    if item["type"] == "operation":
      main_body += generate_operation(item)
    elif item["type"] == "resource":
      main_body += generate_resource_decl(item)
  template = """
// clang-format off
// This file is generated by generate_binding.py
#ifndef LCIXX_BINDING_H_
#define LCIXX_BINDING_H_

namespace lcixx {{
{}

}} // namespace lcixx

#endif // LCIXX_BINDING_H_
"""
  return template.format(main_body)

def generate_impl(input):
  main_body = ""
  for item in input:
    if item["type"] == "resource":
      main_body += generate_resource_impl(item)

  template = """
// clang-format off
// This file is generated by generate_binding.py

#include "lcixx_internal.hpp"

namespace lcixx {{
{}
}} // namespace lcixx
"""
  return template.format(main_body)

def generate_binding(input):
  if os.path.exists(output_dir) == False:
    os.mkdir(output_dir)

  header_path = output_dir + "/lcixx_binding.hpp"
  with open(header_path, "w") as f:
    f.write(generate_header(input))
    print(f"Generated binding header in {os.path.abspath(f.name)}")

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