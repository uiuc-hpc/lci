def optional_args(type, name):
    return (type, name, False)

def positional_args(type, name):
    return (type, name, True)

def attr(type, name, default_var=None, env_var=None):
    return (type, name, default_var, env_var)

def resource(name, attrs):
    return {
        "type": "resource",
        "name": name,
        "attrs": attrs
    }

def operation(name, args, comment=None):
    return {
        "type": "operation",
        "name": name,
        "args": args
    }

def operation_alloc(resource, additiona_args=[], add_runtime_args=True):
    resource_name = resource["name"]
    args = []
    if add_runtime_args:
        args.append(runtime_args)
    for attr in resource["attrs"]:
        args.append(optional_args(attr[0], attr[1]))
    return {
        "type": "operation",
        "name": f"alloc_{resource_name}",
        "args": args + [
            optional_error_args,
            positional_args(f"{resource_name}_t*", resource_name)
        ] + additiona_args
    }

def operation_free(resource, add_runtime_args=True):
    resource_name = resource["name"]
    args = []
    if add_runtime_args:
        args.append(runtime_args)
    return {
        "type": "operation",
        "name": f"free_{resource_name}",
        "args": args + [
            optional_error_args,
            positional_args(f"{resource_name}_t*", resource_name)
        ]
    }

runtime_attr = attr("runtime_t", "runtime")
runtime_args = optional_args("runtime_t", "runtime")
optional_error_args = optional_args("error_t*", "error")
positional_error_args = positional_args("error_t*", "error")