"""
The configure_file rule imitates the similar CMake function for generating code: https://cmake.org/cmake/help/latest/command/configure_file.html
"""

def _cmake_like_configure_file_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file.src,
        output = ctx.outputs.out,
        substitutions = {
            "@" + k + "@": v
            for k, v in ctx.attr.config.items()
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
        "config": attr.string_dict(mandatory = True),
    },
)
