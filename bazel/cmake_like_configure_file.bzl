"""
The configure_file rule imitates the similar CMake function for generating code: https://cmake.org/cmake/help/latest/command/configure_file.html
"""

def _version_dict_from_string(version):
    # Supported formats:
    # - "MAJOR.MINOR.PATCH" - (e.g. "3.5.10")
    # - "MAJOR.MINOR.PATCH.HOTFIX" - (e.g. "3.5.10.1")
    # - "MAJOR.MINOR.PATCH.HOTFIX-apex_suffix" - (e.g. "3.5.10.1-apex1")
    #
    if not version:
        return {}

    apex_version = "apex"
    version_core = version
    if "-" in version:
        version_core, apex_suffix = version.split("-", 1)

        # Be permissive: treat empty / non-numeric apex suffix as-is.
        apex_version = apex_suffix if apex_suffix else "0"

    parts = [p for p in version_core.split(".") if p != ""]
    if len(parts) < 3 or len(parts) > 4:
        fail("version must be 'MAJOR.MINOR.PATCH[.HOTFIX][-apex_suffix]': got '{}'".format(version))

    major, minor, patch = parts[0], parts[1], parts[2]
    hotfix = parts[3] if len(parts) >= 4 else "0"

    return {
        "VSOMEIP_HOTFIX_VERSION": hotfix,
        "VSOMEIP_MAJOR_VERSION": major,
        "VSOMEIP_MINOR_VERSION": minor,
        "VSOMEIP_PATCH_VERSION": patch,
        "VSOMEIP_APEX_VERSION": apex_version,
    }

def _cmake_like_configure_file_impl(ctx):
    config = dict(_version_dict_from_string(ctx.attr.version))
    config.update(ctx.attr.config)

    ctx.actions.expand_template(
        template = ctx.file.src,
        output = ctx.outputs.out,
        substitutions = {
            "@" + k + "@": v
            for k, v in config.items()
        },
    )
    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])
    return [DefaultInfo(files = files, runfiles = runfiles)]

cmake_like_configure_file = rule(
    implementation = _cmake_like_configure_file_impl,
    provides = [DefaultInfo],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        "out": attr.output(mandatory = True),
        "version": attr.string(),
        "config": attr.string_dict(mandatory = True),
    },
)
