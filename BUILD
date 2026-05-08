load("@apex//common/bazel/rules_cc:defs.bzl", "apex_cc_library")

# apex_cc_library(
#     name = "vsomeip3_shared",
#     shared_lib_name = "libvsomeip3.so",
#     tags = ["same-ros-pkg-as: vsomeip3"],
#     # Disabled due to linking problem when used as an external repository with sanitizers enabled.
#     #user_link_flags = [
#     #    "-Wl,--no-undefined",
#     #],
#     deps = [
#         "//implementation:configuration",
#         "//implementation:service_discovery",
#     ],
# )

apex_cc_library(
    name = "vsomeip3_config_plugin",
    tags = ["same-ros-pkg-as: vsomeip3"],
    # Disabled due to linking problem when used as an external repository with sanitizers enabled.
    #user_link_flags = [
    #    "-Wl,--no-undefined",
    #],
    deps = ["//implementation:configuration"],
)

apex_cc_library(
    name = "vsomeip3_sd_plugin",
    tags = ["same-ros-pkg-as: vsomeip3"],
    # Disabled due to linking problem when used as an external repository with sanitizers enabled.
    #user_link_flags = [
    #    "-Wl,--no-undefined",
    #],
    deps = ["//implementation:service_discovery"],
)

# apex_cc_library(
#     name = "vsomeip3_import",
#     shared_library = ":vsomeip3_shared",
#     tags = ["same-ros-pkg-as: vsomeip3"],
#     deps = ["//interface"],
# )


# interface library, use this target to depend on vsomeip
apex_cc_library(
    name = "vsomeip3",
    linkopts = select({
        "@platforms//os:linux": ["-lpthread"],
        "//conditions:default": [],
    }),
    linkstatic = True,  # no object files
    visibility = ["//visibility:public"],
    deps = [
        ":vsomeip3_config_plugin",
        # ":vsomeip3_import",
        ":vsomeip3_sd_plugin",
        "//interface",
    ],
)
