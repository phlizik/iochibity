# the only reason for this is so _sockets.h will be picked up by //:mkhdrs

cc_library(
    name = "sockets",
    srcs = ["_sockets.h"],
    visibility = ["//src/comm:__subpackages__"]
)

cc_library(
    name = "transport",
    srcs = ["_transport.h"],
    visibility = ["//src/sec:__subpackages__"]
)
