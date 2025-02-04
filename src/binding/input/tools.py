def get_enum_attr_type(name):
    return f"attr_{name}_t"

def optional_arg(type, name, default_value, inout_trait="in", comment=""):
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
    type = get_enum_attr_type(name)
    return {
        "category": "attribute",
        "trait": ["enum", inout_trait],
        "default_value": default_value,
        "type": type,
        "enum_options": enum_options,
        "name": name,
        "comment": comment
    }

def attr_to_arg(attr):
    assert "out" not in attr["trait"]
    if "not_global" in attr["trait"]:
        default_value = attr["default_value"]
    else:
        default_value = f"g_default_attr.{attr['name']}"
    return {
        "category": "argument",
        "trait": ["optional"] + attr["trait"],
        "default_value": default_value,
        "type": attr["type"],
        "name": attr["name"],
        "comment": "argument for the corresponding attribute"
    }

def resource(name, attrs, comment=""):
    return {
        "category": "resource",
        "name": name,
        "attrs": attrs + [
            attr("void*", "user_context", comment="User context for the resource.", default_value="nullptr", extra_trait=["not_global"])
            ],
        "comment": comment
    }

def operation(name, args, init_global=False, fina_global=False, comment=""):
    return {
        "category": "operation",
        "name": name,
        "args": args,
        "init_global": init_global,
        "fina_global": fina_global,
        "comment": comment
    }

def operation_alloc(resource, additiona_args=[], add_runtime_args=True, init_global=False, fina_global=False):
    resource_name = resource["name"]
    args = []
    if add_runtime_args:
        args.append(optional_runtime_args)
    for attr in resource["attrs"]:
        if "out" not in attr["trait"]:
            args.append(attr_to_arg(attr))
    args.append(return_val(f"{resource_name}_t", resource_name, comment="The allocated object."))
    return operation(f"alloc_{resource_name}", args + additiona_args, 
                     init_global=init_global, fina_global=fina_global, 
                     comment=f"Allocates a new {resource_name} object.")

def operation_free(resource, add_runtime_args=True, init_global=False, fina_global=False):
    resource_name = resource["name"]
    args = []
    if add_runtime_args:
        args.append(optional_runtime_args)
    args.append(positional_arg(f"{resource_name}_t*", resource_name, inout_trait="inout", comment="The object to free."))
    return operation(f"free_{resource_name}", args, 
                     init_global=init_global, fina_global=fina_global, 
                     comment=f"Frees a {resource_name} object.")

optional_runtime_args = optional_arg("runtime_t", "runtime", default_value="g_default_runtime", comment="The runtime object.")