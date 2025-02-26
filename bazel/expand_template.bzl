"""Repository-local replacement for bazel_skylib's expand_template rule."""

def _expand_template_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.out,
        substitutions = ctx.attr.substitutions,
        is_executable = False,
    )

expand_template = rule(
    implementation = _expand_template_impl,
    attrs = {
        "template": attr.label(allow_single_file = True, mandatory = True),
        "out": attr.output(mandatory = True),
        "substitutions": attr.string_dict(),
    },
)
