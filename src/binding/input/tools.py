# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

def get_enum_attr_type(name):
    return f"attr_{name}_t"

def optional_arg(type, name, default_value=None, inout_trait="in", comment=""):
    return {
        "category": "argument",
        "trait": ["optional", inout_trait],
        "default_value": default_value,
        "type": type,
        "name": name,
        "comment": comment
    }

def positional_arg(type, name, inout_trait="in",comment=""):
    return {
        "category": "argument",
        "trait": ["positional", inout_trait],
        "type": type,
        "name": name,
        "comment": comment
    }

def return_val(type, name, comment=""):
    return {
        "category": "return_val",
        "type": type,
        "name": name,
        "comment": comment
    }

def attr(type, name, default_value=None, inout_trait="inout", extra_trait=None, comment=""):
    if extra_trait is None:
        extra_trait = []
    return {
        "category": "attribute",
        "trait": [inout_trait] + extra_trait,
        "default_value": default_value,
        "type": type,
        "name": name,
        "comment": comment
    }

def attr_enum(name, enum_options, default_value=None, inout_trait="inout", comment=""):
    assert default_value is None or default_value in enum_options
    type = get_enum_attr_type(name)
    return {
        "category": "attribute",
        "trait": ["enum", inout_trait],
        "default_value": f"{type}::{default_value}",
        "type": type,
        "enum_options": enum_options,
        "name": name,
        "comment": comment
    }

def get_attr_default_value(resource_name, attr_name = "attr"):
    return f"g_default_attr.{attr_name}"

def attr_to_arg(resource_name, attr):
    assert "out" not in attr["trait"]
    default_value = get_attr_default_value(resource_name, attr["name"])
    if "no_env_config" in attr["trait"]:
        default_value = attr["default_value"]
    return {
        "category": "argument",
        "trait": ["optional"] + attr["trait"],
        "default_value": default_value,
        "type": attr["type"],
        "name": attr["name"],
        "comment": "argument for the corresponding attribute"
    }

def resource(name, attrs, children=[], doc={}, custom_is_empty_method=False):
    return {
        "category": "resource",
        "name": name,
        "attrs": attrs + [
            attr("const char*", "name", comment="Name of the resource.", default_value="DEFAULT_NAME", extra_trait=["no_env_config"]),
            attr("void*", "user_context", comment="User context for the resource.", default_value="nullptr", extra_trait=["no_env_config"])
            ],
        "children": children,
        "custom_is_empty_method": custom_is_empty_method,
        "doc": doc
    }

def operation(name, args, init_global=False, fina_global=False, comment="", doc={}):
    return {
        "category": "operation",
        "name": name,
        "args": args,
        "init_global": init_global,
        "fina_global": fina_global,
        "comment": comment,
        "doc": doc
    }

def operation_alloc(resource, additional_args=[], add_runtime_args=True, init_global=False, fina_global=False):
    resource_name = resource["name"]
    args = []
    for attr in resource["attrs"]:
        if "out" in attr["trait"]:
            continue
        args.append(attr_to_arg(resource_name, attr))
    if add_runtime_args:
        args.append(optional_runtime_args)
    args.append(return_val(f"{resource_name}_t", resource_name, comment="The allocated object."))
    in_group = "LCI_OTHER"
    if "in_group" in resource["doc"]:
        in_group = resource["doc"]["in_group"]
    return operation(f"alloc_{resource_name}", args + additional_args, 
                     init_global=init_global, fina_global=fina_global, 
                     doc={"in_group": in_group, "brief": f"Allocate a new {resource_name} object."})

def operation_free(resource, add_runtime_args=True, init_global=False, fina_global=False):
    resource_name = resource["name"]
    args = []
    if add_runtime_args:
        args.append(optional_runtime_args)
    args.append(positional_arg(f"{resource_name}_t*", resource_name, inout_trait="inout", comment="The object to free."))
    in_group = "LCI_OTHER"
    if "in_group" in resource["doc"]:
        in_group = resource["doc"]["in_group"]
    return operation(f"free_{resource_name}", args, 
                     init_global=init_global, fina_global=fina_global, 
                     doc={"in_group": in_group, "brief": f"Free a new {resource_name} object."})

optional_runtime_args = optional_arg("runtime_t", "runtime", default_value="g_default_runtime", comment="The runtime object.")