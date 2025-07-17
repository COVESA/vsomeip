# ----------------------------------
# Options affecting listfile parsing
# ----------------------------------
with section("parse"):
    # Specify structure for custom cmake functions

    _target_sources_files = {
        "kwargs": {
            "FILE_SET": {
                "pargs": {"flags": ["HEADERS"]},
                "kwargs": {
                    "TYPE": "1",
                    "BASE_DIRS": "+",
                    "FILES": "+",
                },
            }
        },
    }

    _install_type_args = {
        "pargs": {
            "nargs": "+",
            "flags": ["RUNTIME", "ARCHIVE", "LIBRARY", "OBJECTS"],
        },
        "kwargs": {
            "DESTINATION": {"pargs": {"nargs": 1}},
            "EXPORT": {"pargs": {"nargs": 1}},
            "NAMESPACE": {"pargs": {"nargs": 1}},
            "COMPONENT": {"pargs": {"nargs": 1}},
            "EXCLUDE_FROM_ALL": {"pargs": {"nargs": 0}},
            "PERMISSIONS": {
                "pargs": {
                    "nargs": "+",
                    "flags": [
                        "OWNER_READ",
                        "OWNER_WRITE",
                        "OWNER_EXECUTE",
                        "GROUP_READ",
                        "GROUP_EXECUTE",
                        "WORLD_READ",
                        "WORLD_EXECUTE",
                    ],
                }
            },
        },
    }
    _files_install_type_args = _install_type_args.copy()
    _files_install_type_args["kwargs"]["TYPE"] = {
        "pargs": {"nargs": 1, "flags": ["SYSCONF", "BIN"]}
    }
    _directory_install_type_args = _install_type_args.copy()
    _directory_install_type_args["kwargs"]["EXCLUDE PATTERN"] = {"pargs": {"nargs": 1}}
    _directory_install_type_args["kwargs"]["PATTERN"] = {"pargs": {"nargs": 1}}

    additional_commands = {
        "file": {
            "kwargs": {
                "GENERATE": {
                    "kwargs": {
                        "OUTPUT": 1,
                        "CONTENT": 1,
                        "NEWLINE_STYLE": {
                            "pargs": {
                                "nargs": 1,
                            },
                        },
                    }
                }
            }
        },
        "add_library": {
            "pargs": {
                "nargs": "+",
                "flags": ["SHARED", "STATIC", "EXCLUDE_FROM_ALL"],
            }
        },
        "target_sources": {
            "pargs": {
                "nargs",
                1,
            },
            "kwargs": {
                "PRIVATE": _target_sources_files,
                "PUBLIC": _target_sources_files,
            },
        },
        "target_link_libraries": {
            "pargs": "*",
            "kwargs": {"PUBLIC": "*", "PRIVATE": "*", "INTERFACE": "*"},
        },
        "target_include_directories": {
            "pargs": "*",
            "flags": ["SYSTEM"],
            "kwargs": {"PUBLIC": "*", "PRIVATE": "*", "INTERFACE": "*"},
        },
        "set_target_properties": {
            "pargs": "*",
            "flags": ["PROPERTIES"],
            "kwargs": {
                "OUTPUT_NAME": "1",
                "EXPORT_NAME": "1",
                "LINKER_LANGUAGE": "1",
                "CMAKE_CXX_STANDARD_REQUIRED": "1",
                "COMPILE_DEFINITIONS": "1",
                "COMPILE_OPTIONS": "1",
                "VERSION": "1",
                "SOVERSION": "1",
            },
        },
        "SET_UP_GEN_FILES": {
            "kwargs": {
                "ADDITIONAL_OUTPUTS": "+",
                "FDEPL_FILE": 1,
                "FIDL_FILE": 1,
                "INSTALL_HEADERS_DIR": 1,
                "INTERFACES": "+",
                "MIDDLEWARE": 1,
                "OUTPUT_DIR": 1,
                "TOP_PREFIX": 1,
                "VAR_PREFIX": 1,
                "VAR_PREFIXES": "+",
                "VERSION": 1,
                "VERSIONS": "+",
            },
            "pargs": {
                "flags": ["DO_CORE_RERUN_WITH_FDEPL", "NO_CLOBBER"],
                "nargs": "*",
            },
        },
        "CREATE_VSOMEIP_CONFIG_FILE": {
            "kwargs": {
                "INSTANCE_ID": "*",
                "VSOMEIP_JSON_FILE": "*",
                "KEY": "*",
                "JSON_GLOBS": "*",
            }
        },
        "install": {
            "kwargs": {
                "TARGETS": _install_type_args,
                "FILES": _files_install_type_args,
                "DIRECTORY": _directory_install_type_args,
                "INCLUDES DESTINATION": "*",
                "FILES_MATCHING": {
                    "kwargs": {
                        "PATTERN": "*",
                        "EXCLUDE PATTERN": "*",
                    }
                },
            },
        },
        "list": {
            # "pargs": "*",
            "kwargs": {
                "APPEND": "1",
                "FILTER": "1",
                # "INSERT": "1",
                # "POP_BACK": "1",
            },
        },
    }

    # Override configurations per-command where available
    override_spec = {}

    # Specify variable tags.
    vartags = []

    # Specify property tags.
    proptags = []

# -----------------------------
# Options affecting formatting.
# -----------------------------
with section("format"):
    # Disable formatting entirely, making cmake-format a no-op
    disable = False

    # How wide to allow formatted cmake files
    line_width = 120

    # How many spaces to tab for indent
    tab_size = 2

    # If true, lines are indented using tab characters (utf-8 0x09) instead of
    # <tab_size> space characters (utf-8 0x20). In cases where the layout would
    # require a fractional tab character, the behavior of the  fractional
    # indentation is governed by <fractional_tab_policy>
    use_tabchars = False

    # If <use_tabchars> is True, then the value of this variable indicates how
    # fractional indentions are handled during whitespace replacement. If set to
    # 'use-space', fractional indentation is left as spaces (utf-8 0x20). If set
    # to `round-up` fractional indentation is replaced with a single tab character
    # (utf-8 0x09) effectively shifting the column to the next tabstop
    fractional_tab_policy = "use-space"

    # If an argument group contains more than this many sub-groups (parg or kwarg
    # groups) then force it to a vertical layout.
    max_subgroups_hwrap = 2

    # If a positional argument group contains more than this many arguments, then
    # force it to a vertical layout.
    max_pargs_hwrap = 3

    # If a cmdline positional group consumes more than this many lines without
    # nesting, then invalidate the layout (and nest)
    max_rows_cmdline = 2

    # If true, separate flow control names from their parentheses with a space
    separate_ctrl_name_with_space = False

    # If true, separate function names from parentheses with a space
    separate_fn_name_with_space = False

    # If a statement is wrapped to more than one line, than dangle the closing
    # parenthesis on its own line.
    dangle_parens = True

    # If the trailing parenthesis must be 'dangled' on its on line, then align it
    # to this reference: `prefix`: the start of the statement,  `prefix-indent`:
    # the start of the statement, plus one indentation  level, `child`: align to
    # the column of the arguments
    dangle_align = "prefix"

    # If the statement spelling length (including space and parenthesis) is
    # smaller than this amount, then force reject nested layouts.
    min_prefix_chars = 4

    # If the statement spelling length (including space and parenthesis) is larger
    # than the tab width by more than this amount, then force reject un-nested
    # layouts.
    max_prefix_chars = 10

    # If a candidate layout is wrapped horizontally but it exceeds this many
    # lines, then reject the layout.
    max_lines_hwrap = 2

    # What style line endings to use in the output.
    line_ending = "unix"

    # Format command names consistently as 'lower' or 'upper' case
    command_case = "canonical"

    # Format keywords consistently as 'lower' or 'upper' case
    keyword_case = "unchanged"

    # A list of command names which should always be wrapped
    always_wrap = []

    # If true, the argument lists which are known to be sortable will be sorted
    # lexicographicall
    enable_sort = True

    # If true, the parsers may infer whether or not an argument list is sortable
    # (without annotation).
    autosort = False

    # By default, if cmake-format cannot successfully fit everything into the
    # desired linewidth it will apply the last, most agressive attempt that it
    # made. If this flag is True, however, cmake-format will print error, exit
    # with non-zero status code, and write-out nothing
    require_valid_layout = False

    # A dictionary mapping layout nodes to a list of wrap decisions. See the
    # documentation for more information.
    layout_passes = {}

# ------------------------------------------------
# Options affecting comment reflow and formatting.
# ------------------------------------------------
with section("markup"):
    # What character to use for bulleted lists
    bullet_char = "*"

    # What character to use as punctuation after numerals in an enumerated list
    enum_char = "."

    # If comment markup is enabled, don't reflow the first comment block in each
    # listfile. Use this to preserve formatting of your copyright/license
    # statements.
    first_comment_is_literal = False

    # If comment markup is enabled, don't reflow any comment block which matches
    # this (regex) pattern. Default is `None` (disabled).
    literal_comment_pattern = None

    # Regular expression to match preformat fences in comments default=
    # ``r'^\s*([`~]{3}[`~]*)(.*)$'``
    fence_pattern = "^\\s*([`~]{3}[`~]*)(.*)$"

    # Regular expression to match rulers in comments default=
    # ``r'^\s*[^\w\s]{3}.*[^\w\s]{3}$'``
    ruler_pattern = "^\\s*[^\\w\\s]{3}.*[^\\w\\s]{3}$"

    # If a comment line matches starts with this pattern then it is explicitly a
    # trailing comment for the preceeding argument. Default is '#<'
    explicit_trailing_pattern = "#<"

    # If a comment line starts with at least this many consecutive hash
    # characters, then don't lstrip() them off. This allows for lazy hash rulers
    # where the first hash char is not separated by space
    hashruler_min_length = 10

    # If true, then insert a space between the first hash char and remaining hash
    # chars in a hash ruler, and normalize its length to fill the column
    canonicalize_hashrulers = True

    # enable comment markup parsing and reflow
    enable_markup = True

# ----------------------------
# Options affecting the linter
# ----------------------------
with section("lint"):
    # a list of lint codes to disable
    disabled_codes = []

    # regular expression pattern describing valid function names
    function_pattern = "[0-9a-z_]+"

    # regular expression pattern describing valid macro names
    macro_pattern = "[0-9A-Z_]+"

    # regular expression pattern describing valid names for variables with global
    # (cache) scope
    global_var_pattern = "[A-Z][0-9A-Z_]+"

    # regular expression pattern describing valid names for variables with global
    # scope (but internal semantic)
    internal_var_pattern = "_[A-Z][0-9A-Z_]+"

    # regular expression pattern describing valid names for variables with local
    # scope
    local_var_pattern = "[a-z][a-z0-9_]+"

    # regular expression pattern describing valid names for privatedirectory
    # variables
    private_var_pattern = "_[0-9a-z_]+"

    # regular expression pattern describing valid names for public directory
    # variables
    public_var_pattern = "[A-Z][0-9A-Z_]+"

    # regular expression pattern describing valid names for function/macro
    # arguments and loop variables.
    argument_var_pattern = "[a-z][a-z0-9_]+"

    # regular expression pattern describing valid names for keywords used in
    # functions or macros
    keyword_pattern = "[A-Z][0-9A-Z_]+"

    # In the heuristic for C0201, how many conditionals to match within a loop in
    # before considering the loop a parser.
    max_conditionals_custom_parser = 2

    # Require at least this many newlines between statements
    min_statement_spacing = 1

    # Require no more than this many newlines between statements
    max_statement_spacing = 2
    max_returns = 6
    max_branches = 12
    max_arguments = 5
    max_localvars = 15
    max_statements = 50

# -------------------------------
# Options affecting file encoding
# -------------------------------
with section("encode"):
    # If true, emit the unicode byte-order mark (BOM) at the start of the file
    emit_byteorder_mark = False

    # Specify the encoding of the input file. Defaults to utf-8
    input_encoding = "utf-8"

    # Specify the encoding of the output file. Defaults to utf-8. Note that cmake
    # only claims to support utf-8 so be careful when using anything else
    output_encoding = "utf-8"

# -------------------------------------
# Miscellaneous configurations options.
# -------------------------------------
with section("misc"):
    # A dictionary containing any per-command configuration overrides. Currently
    # only `command_case` is supported.
    per_command = {}
