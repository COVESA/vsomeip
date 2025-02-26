cc_library(
    name = "interface",
    hdrs = glob(
        [
            "**/*.hpp",
            "**/*.h",
        ],
    ),
    strip_include_prefix = ".",
    tags = ["same-ros-pkg-as: @vsomeip//:vsomeip3"],
    visibility = [
        "//:__pkg__",
        "//implementation:__pkg__",
    ],
)
