load("@vsomeip//bazel:version.bzl", "version_dict_from_string")
load("@bazel_skylib//rules:expand_template.bzl", "expand_template")

APEX_VSOMEIP_VERSION_TAG = "3.4.11-dev1"  # This version must match the git tag version.

expand_template(
    name = "config",
    template = "configuration/include/internal.hpp.in",
    out = "configuration/include/internal.hpp",
    substitutions = 
{
        "@DEFAULT_CONFIGURATION_FILE@": "/etc/vsomeip.json",
        "@DEFAULT_CONFIGURATION_FOLDER@": "/etc/vsomeip",
        "@VSOMEIP_UNICAST_ADDRESS@": "127.0.0.1",
        "@VSOMEIP_DIAGNOSIS_ADDRESS@": "0x01",
        "@VSOMEIP_ROUTING_READY_MESSAGE@": "SOME/IP routing ready.",
        "@VSOMEIP_LOCAL_TCP_PORT_WAIT_TIME@": "100",
        "@VSOMEIP_LOCAL_TCP_PORT_MAX_WAIT_TIME@": "10000",
    } 
    | select({
        "//conditions:default": {"@VSOMEIP_BASE_PATH@": "/tmp"},
        "@platforms//os:qnx": {"@VSOMEIP_BASE_PATH@": "/var"},
    })
    | version_dict_from_string(APEX_VSOMEIP_VERSION_TAG),
)

cc_library(
    name = "generated_config",
    hdrs = [":config"],
    strip_include_prefix = "configuration/include",
)

cc_library(
    name = "helper",
    hdrs = glob(
        [
            "helper/**/*.hpp",
            "helper/**/*.ipp",
        ],
    ),
    defines = [
        "VSOMEIP_BOOST_VERSION=108100",
    ],
    strip_include_prefix = "helper",
    tags = ["same-ros-pkg-as: @vsomeip//:vsomeip3"],
    deps = [
        "@boost//:headers",
        "@boost//:thread",
    ],
)

cc_library(
    name = "configuration",
    srcs = glob(
        [
            "configuration/src/*.cpp",
        ],
    ),
    visibility = ["//:__pkg__"],
    deps = [
        ":implementation",
        "@boost//:filesystem",
    ],
)

cc_library(
    name = "service_discovery",
    srcs = glob(
        [
            "service_discovery/src/*.cpp",
        ],
    ),
    visibility = ["//:__pkg__"],
    deps = [
        ":implementation",
    ],
)

cc_library(
    name = "implementation",
    srcs = glob(
        [
            "endpoints/src/*.cpp",
            "logger/src/*.cpp",
            "tracing/src/*.cpp",
            "message/src/*.cpp",
            "plugin/src/*.cpp",
            "protocol/src/*.cpp",
            "routing/src/*.cpp",
            "runtime/src/*.cpp",
            "security/src/*.cpp",
            "utility/src/*.cpp",
        ],
        exclude = [
            "utility/src/wrappers.cpp",
            "utility/src/wrappers_qnx.cpp",
        ],
    ),
    hdrs = glob(
        [
            "*/include/**/*.hpp",
        ],
    ),
    copts = ["-Wno-error"],
    defines = [
        "WITHOUT_SYSTEMD",
        "VSOMEIP_DISABLE_SECURITY",
    ],
    includes = glob(
        [
            "*/include",
        ],
    ) + ["message"],
    linkopts = select({
        "@platforms//os:linux": [
            "-lpthread",
            "-ldl",
            # these cause linker issues when used as external dependency in gcc8 (tested working with clang16)
            # since CLOEXEC is not immediately necessary for our use-case, disable the wrappers for now
            #"-Wl,-wrap,socket",
            #"-Wl,-wrap,accept",
        ],
        "//conditions:default": [],
    }),
    local_defines = [
        "VSOMEIP_INTERNAL_SUPPRESS_DEPRECATED",
    ],
    tags = ["same-ros-pkg-as: @vsomeip//:vsomeip3"],
    visibility = ["//:__pkg__"],
    deps = [
        ":generated_config",
        ":helper",
        "//interface",
    ],
)
