load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "headers",
    hdrs = glob(
        [
            "boost/**",
        ],
    ),
    local_defines = ["BOOST_ALL_NO_LIB"],
    include_prefix = ".",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "_thread_detail",
    hdrs = select({
        "@platforms//os:linux": ["libs/thread/src/pthread/once_atomic.cpp"],
        "//conditions:default": [],
    }),
    strip_include_prefix = "libs/thread/src/pthread",
)

cc_library(
    name = "thread",
    srcs = glob(
        [
            "libs/thread/src/*.cpp",
        ],
    ) + select({
        "@platforms//os:linux": [
            "libs/thread/src/pthread/once.cpp",
            "libs/thread/src/pthread/thread.cpp",
        ],
        "//conditions:default": [],
    }),
    implementation_deps = [
        ":_thread_detail",
    ],
    linkopts = select({
        "@platforms//os:linux": ["-lpthread"],
        "//conditions:default": [],
    }),
    deps = [
        ":headers",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "filesystem",
    srcs = glob(
        [
            "libs/filesystem/src/**",
        ],
    ),
    defines = [
        "BOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF"
    ],
    deps = [
        ":headers",
    ],
    visibility = ["//visibility:public"],
)
