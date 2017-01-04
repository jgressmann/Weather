/* The MIT License (MIT)
 * Copyright (c) 2015 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <toe/cmdlopt.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#   include "Windows/getopt.h"
#else
#   include <getopt.h>
#endif

static
const char* s_AppName = "<no app name set>";
static
const char* s_AppVersion = "<no app version set>";
static
const char* s_Header;
static
const char* s_Footer;
static
int s_OptionIndent = 2;
static
int s_ArgIndent = 8;
static
int s_DescriptionIndent = 10;
static
const cmdlopt_opt* s_Options;

static
void
PrintLinesWithIndend(int indent, FILE* file, const char* text) {
    while (*text) {
        fprintf(file, "%c", *text);

        if (*text == '\n') {
            fprintf(file, "%*s", indent, "");
        }

        ++text;
    }

    fprintf(file, "\n");
}


extern
int
cmdlopt_set_help_indents(int optindent, int argindent, int descindent) {
    if (optindent < 0 || argindent < 0 || descindent < 0) {
        return CMDLOPT_E_INVALID_PARAM;
    }

    s_OptionIndent = optindent;
    s_ArgIndent = argindent;
    s_DescriptionIndent = descindent;

    return CMDLOPT_E_NONE;
}


extern
int
cmdlopt_set_app_name(const char* name) {
    if (!name) {
        return CMDLOPT_E_INVALID_PARAM;
    }

    s_AppName = name;

    return CMDLOPT_E_NONE;
}


extern
int
cmdlopt_set_app_version(const char* version) {
    if (!version) {
        return CMDLOPT_E_INVALID_PARAM;
    }

    s_AppVersion = version;

    return CMDLOPT_E_NONE;
}

extern
int
cmdlopt_set_options(const cmdlopt_opt* options) {
    s_Options = options;
    return CMDLOPT_E_NONE;
}

extern
int
cmdlopt_print_help() {
    return cmdlopt_fprint_help(stdout);
}

extern
int
cmdlopt_fprint_help(FILE* file) {
    const cmdlopt_opt* opt = NULL;

    if (!file) {
        return CMDLOPT_E_INVALID_PARAM;
    }

    if (s_Footer) {
        fprintf(file, "%s\n", s_Footer);
    }

    if (s_Header) {
        fprintf(file, "%s\n", s_Header);
    } else {
        fprintf(file, "Usage: %s%s\n\n", s_AppName, s_Options ? " [OPTIONS]" : "");
    }

    if (!s_Options) {
        return CMDLOPT_E_NONE;
    }

    for (opt = s_Options; opt->name; ++opt) {
        if (opt->getopt_char) {
            fprintf(file, "  -%c, ", opt->getopt_char);
        } else {
            fprintf(file, "  ");
        }

        fprintf(file, "--%s: %s\n", opt->name, opt->description ? opt->description : "");

        if (opt->args) {
            const cmdlopt_arg* arg = opt->args;

            for (; arg->name || arg->description || arg->example; ++arg) {
                const int Indent = 4;

                if (arg->name) {
                    if (arg->description) {
                        fprintf(file, "%*s%s: ", Indent, "", arg->name);
                        PrintLinesWithIndend(Indent, file, arg->description);
                    } else {
                        fprintf(file, "%*s%s\n", Indent, "", arg->name);
                    }
                } else { // no name
                    if (arg->description) {
                        PrintLinesWithIndend(Indent, file, arg->description);
                    }
                }

                if (arg->example) {
                    fprintf(file, "%*sExample: %s\n", Indent + 6, "", arg->example);
                }
            }

            fprintf(file, "\n");
        }
    }

    fprintf(file, "\n");

    if (s_Footer) {
        fprintf(file, "%s\n", s_Footer);
    }

    return CMDLOPT_E_NONE;
}

extern
int
cmdlopt_parse_cmdl(int argc, char** argv, void* ctx) {
    char* getoptString = NULL;
    struct option* longOptions = NULL;
    size_t optionCount = 0, getoptStringOffset = 0, i = 0;
    int error = CMDLOPT_E_NONE;
    const cmdlopt_opt* opt = NULL;

    if (argc <= 0 || argv == NULL) {
        error = CMDLOPT_E_INVALID_PARAM;
        goto Exit;
    }

    if (!s_Options) {
        goto Exit;
    }

    for (opt = s_Options; opt->name; ++opt, ++optionCount);

    getoptString = (char*)malloc(optionCount * 2 + 1);
    longOptions = (struct option*)malloc(sizeof(*longOptions) * (optionCount + 1));


    for (i = 0; i < optionCount + 1; ++i) {
        longOptions[i].name = s_Options[i].name;
        longOptions[i].has_arg = s_Options[i].args ? required_argument : no_argument;
        longOptions[i].flag = NULL;
        longOptions[i].val = s_Options[i].getopt_long_value;

        if (s_Options[i].getopt_char) {
            getoptString[getoptStringOffset++] = s_Options[i].getopt_char;

            if (s_Options[i].args) {
                getoptString[getoptStringOffset++] = ':';
            }
        }
    }

    getoptString[getoptStringOffset] = 0;

    /* Reset getopt */
    optarg = NULL;
    optind = 1;
    opterr = 0; /* print error message */
    optopt = 0;

    for (;;) {
        const int c = getopt_long(argc, argv, getoptString, longOptions, NULL);

        if (c == -1) {
            break;
        }

        switch (c) {
        case CMDLOPT_HELP_GETOPT_LONG_VALUE:
        case 'h':
            cmdlopt_print_help();
            error = CMDLOPT_E_HELP_REQUESTED;
            goto Exit;
        case CMDLOPT_VERSION_GETOPT_LONG_VALUE:
            fprintf(stdout, "%s version %s\n\n", s_AppName, s_AppVersion);
            error = CMDLOPT_E_VERSION_REQUESTED;
            goto Exit;
        default: {
            int found = 0;

            for (opt = s_Options; opt->name; ++opt) {
                if (opt->getopt_char == c || opt->getopt_long_value == c) {
                    found = 1;

                    if (opt->arg_parser) {
                        error = opt->arg_parser(ctx, optarg);

                        if (error != CMDLOPT_E_NONE) {
                            goto Exit;
                        }
                    }
                    break;
                }
            }

            if (!found) {
                cmdlopt_print_help();
                error = CMDLOPT_E_UNKNOWN_OPTION;
                goto Exit;
            }
        } break;
        }
    }

Exit:
    free(getoptString);
    free(longOptions);
    return error;
}


extern
int
cmdlopt_set_help_header(const char* header) {
    s_Header = header;
    return CMDLOPT_E_NONE;
}

/* Sets a custom footer to show after the options are printed. */
extern
int
cmdlopt_set_help_footer(const char* footer) {
    s_Footer = footer;
    return CMDLOPT_E_NONE;
}

