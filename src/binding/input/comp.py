# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

import sys, os
sys.path.append(os.path.dirname(__file__))
from .tools import *

input = [
# comp
resource_comp := resource(
    "comp", 
    [
        attr_enum("comp_type", enum_options=["sync", "cq", "handler", "graph"], default_value="cq", comment="The completion object type.", inout_trait="out"),
        attr("int", "sync_threshold", default_value=1, comment="The threshold for sync (synchronizer).", inout_trait="out"),
        attr("bool", "zero_copy_am", default_value="false", comment="Whether to directly pass internal packet into the completion object."),
        attr_enum("cq_type", enum_options=["array_atomic", "lcrq"], default_value="lcrq", comment="The completion object type."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "The completion object resource.",
    }
),
operation_free(resource_comp),
operation(
    "comp_signal", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion object to signal."),
        positional_arg("status_t", "status", comment="The status to signal."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Signal a completion object.",
    }
),
operation(
    "reserve_rcomps", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "n", comment="The number of remote completion handler slots to reserve."),
        return_val("rcomp_t", "start", comment="The start of the reserved slots."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Reserve spaces for n remote completion handlers.",
    }
),
operation(
    "register_rcomp", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion object to register."),
        optional_arg("rcomp_t", "rcomp", "0", comment="The remote completion handler slot used to register."),
        return_val("rcomp_t", "rcomp_out", comment="The remote completion handler pointing to the completion object."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Register a completion object into a remote completion handler.",
    }
),
operation(
    "deregister_rcomp", 
    [
        optional_runtime_args,
        positional_arg("rcomp_t", "rcomp", comment="The remote completion handler to deregister."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Deregister a remote completion handler.",
    }
),
# sync
operation(
    "alloc_sync", 
    [
        optional_runtime_args,
        optional_arg("int", "threshold", 1, comment="The signaling threshold of the synchronizer."),
        optional_arg("bool", "zero_copy_am", "false", comment="Whether to directly pass internal packet into the completion object."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated synchronizer.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a synchronizer.",
    }
),
operation(
    "sync_test", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The synchronizer to test."),
        positional_arg("status_t*", "p_out", inout_trait="out", comment="The pointer to an status array of size `comp.get_attr_sync_threshold()` to hold the outputs (only valid if the function returns *true*)."),
        return_val("bool", "succeed", comment="Whether the synchronizer is ready.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Test a synchronizer.",
    }
),
operation(
    "sync_wait", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The synchronizer to wait."),
        positional_arg("status_t*", "p_out", inout_trait="out", comment="The pointer to an status array of size `comp.get_attr_sync_threshold()` to hold the outputs."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Wait for a synchronizer to be ready.",
    }
),
# cq
operation(
    "alloc_cq", 
    [
        optional_runtime_args,
        optional_arg("int", "default_length", "65536", comment="The default length of the completion queue."),
        optional_arg("bool", "zero_copy_am", "false", comment="Whether to directly pass internal packet into the completion object."),
        optional_arg("attr_cq_type_t", "cq_type", "attr_cq_type_t::array_atomic", comment="The type of the completion queue."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated completion queue.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a completion queue.",
    }
),
operation(
    "cq_pop", 
    [
        optional_runtime_args,
        positional_arg("comp_t", "comp", comment="The completion queue to pop."),
        return_val("status_t", "status", comment="The popped status.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Pop a status from a completion queue.",
        "details": "This function is a nonblocking operation. It can return a status with an error code of either *retry* or *ok*. Other fields of the status are only valid if the error code is *ok*."
    }
),
# handler
operation(
    "alloc_handler", 
    [
        optional_runtime_args,
        positional_arg("comp_handler_t", "handler", comment="The handler function."),
        optional_arg("bool", "zero_copy_am", "false", comment="Whether to directly pass internal packet into the completion object."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        return_val("comp_t", "comp", comment="The allocated completion handler.")
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a completion handler.",
    }
),
# graph
operation(
    "alloc_graph", 
    [
        optional_arg("comp_t", "comp", "comp_t()", comment="Another completion object to signal when the graph is completed. The graph will be automatically destroyed afterwards."),
        optional_arg("void*", "user_context", "nullptr", comment="The abitrary user-defined context associated with this completion object."),
        optional_runtime_args,
        return_val("comp_t", "comp", comment="The allocated completion handler."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Allocate a completion handler.",
    },
),
operation(
    "graph_add_node", 
    [
        positional_arg("comp_t", "comp", comment="The graph to add node."),
        positional_arg("graph_node_fn_t", "fn", comment="The function to run when the node is triggered."),
        optional_arg("void*", "value", "nullptr", comment="The abitrary user-defined value associated with this node."),
        optional_runtime_args,
        return_val("graph_node_t", "node", comment="A handler representing the node added to the graph."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Add a node to a graph.",
    },
),
operation(
    "graph_add_edge", 
    [
        positional_arg("comp_t", "comp", comment="The graph to add node."),
        positional_arg("graph_node_t", "src", comment="The src node."),
        positional_arg("graph_node_t", "dst", comment="The dst node."),
        optional_arg("graph_edge_fn_t", "fn", "nullptr", comment="The function to run when the edge is triggered."),
        optional_runtime_args,
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Add a edge to a graph.",
    },
),
operation(
    "graph_node_mark_complete",
    [
        positional_arg("graph_node_t", "node", comment="The node to mark complete."),
        optional_arg("status_t", "status", "status_t()", comment="The status to mark complete with."),
        optional_runtime_args,
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Mark a node complete.",
    }
),
operation(
    "graph_start",
    [
        positional_arg("comp_t", "comp", comment="The graph to start."),
        optional_runtime_args,
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Start a graph.",
    }
),
operation(
    "graph_test",
    [
        positional_arg("comp_t", "comp", comment="The graph to test."),
        optional_runtime_args,
        return_val("status_t", "status", comment="The status of the graph."),
    ],
    doc = {
        "in_group": "LCI_COMPLETION",
        "brief": "Test a graph.",
        "details": "Successful test will reset the graph to the state that is ready to be started again.",
    }
)
]

def get_input():
    return input