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

#ifndef TOE_CMDLOPT_H
#define TOE_CMDLOPT_H

#include <stdio.h>
#include <toe/dll.h>

/* Command line parsing and option processing library.
 *
 * Example:
 * static
 * int
 * ParseSource(void*, const char* optarg);
 *
 * static const cmdlopt_arg SourceArg[] = {
 *    { NULL, "4-byte identifier of the packet source in hex chars. Defaults to 00000000", "001B8101" },
 *    { NULL, NULL, NULL }
 * };
 * const cmdlopt_opt g_Commands[] = {
 *    { "source", 's', 0x100, "Source of the packet", SourceArg, ParseSource},
 *    CMDLOPT_COMMON_OPTIONS,
 *    { NULL, NULL, 0, 0, NULL, NULL },
 *  };
 *
 * int
 * main(int argc, char** argv)
 * {
 *     cmdlopt_set_help_indents(2, 8, 10);
 *     cmdlopt_set_app_name("myapp");
 *     cmdlopt_set_app_version("1.0\nCopyright (C) 2014 Jean Gressmann (jean@0x42.de)");
 *     cmdlopt_set_options(g_Commands);
 *     int error = cmdlopt_parse_cmdl(argc, argv, NULL);
 *     switch (error)
 *     {
 *     case CMDLOPT_E_NONE:
 *          break;
 *     ....
 *     }
 * }
 */
#ifdef __cplusplus
extern "C" {
#endif

/* Argument of an option
 *
 * Not all members need be set.
 * Set all members to NULL to mark the end of array.
 * See cmdlopt_opt for details
 */
typedef struct _cmdlopt_arg {
    /* Name of the argument. Can be NULL */
    const char* name;
    /* Description of the argument / command. Can be NULL */
    const char* description;
    /* An example how to use the option. Can be NULL */
    const char* example;
} cmdlopt_arg;

/* Callback for options that take an argument
 *
 * Should return 0 if the option was successfully parsed.
 *
 * If the return value is not zero, this value is returned from
 * cmdlopt_parse_cmdl.
 */
typedef int (*cmdlopt_arg_parser_t)(void* ctx, char* arg);

typedef struct _cmdlopt_opt {
    /* Option/Command name.
     * Must not be NULL unless its the end-of-options array entry. */
    const char* name;
    /* Description of the option */
    const char* description;
    /* Char to use in getopt call. Can be 0 to skip. */
    char getopt_char;
    /* Value for getopt_long. Can be 0 to skip. */
    int getopt_long_value;
    /* Pointer to option argument array.
     * Set to NULL if the option has no arguments. */
    const cmdlopt_arg* args;
    /* Parser called for each option argument.
     * Set to NULL if no parsing is required. */
    cmdlopt_arg_parser_t arg_parser;
} cmdlopt_opt;



/* Error codes */
#define CMDLOPT_E_NONE 0                /* No error */
#define CMDLOPT_E_HELP_REQUESTED -1     /* Help option was requested */
#define CMDLOPT_E_VERSION_REQUESTED -2  /* Version option was requested */
#define CMDLOPT_E_UNKNOWN_OPTION -3     /* Option passed on the command line wasn't registered. */
#define CMDLOPT_E_INVALID_PARAM -4      /* Version option was requested */
#define CMDLOPT_E_ERRNO -5              /* Check errno variable for error code */

/* Predefined options */
#define CMDLOPT_VERSION_GETOPT_LONG_VALUE   -3
#define CMDLOPT_HELP_GETOPT_LONG_VALUE      -2

#define CMDLOPT_VERSION_OPTION  { \
    "version", \
    "Prints the version of this program", \
    '\0', \
    (int)CMDLOPT_VERSION_GETOPT_LONG_VALUE, \
    NULL, \
    NULL }

#define CMDLOPT_HELP_OPTION { \
    "help", \
    "Prints this help", \
    'h', \
    (int)CMDLOPT_HELP_GETOPT_LONG_VALUE, \
    NULL, \
    NULL }

/* All predefined options */
#define CMDLOPT_COMMON_OPTIONS CMDLOPT_VERSION_OPTION, CMDLOPT_HELP_OPTION

/* Terminator entry to place at end of options array */
#define CMDLOPT_OPTION_TERMINATOR { NULL, NULL, 0, 0, NULL, NULL }

/* Terminator entry to place at end of option argument array */
#define CMDLOPT_ARGUMENT_TERMINATOR { NULL, NULL, NULL }

/* Parses the command line. Output is printed to stdout.
 * Non-options are left at the end. See getopt_long for details. */
TOE_DLL_API
int
cmdlopt_parse_cmdl(int arg, char** argv, void* ctx);

/* Prints help to stdout */
TOE_DLL_API
int
cmdlopt_print_help();

/* Prints help to stream */
TOE_DLL_API
int
cmdlopt_fprint_help(FILE*);

/* Sets indents for help printing.
 *
 * Defaults to 2 for options and 8 for arguments */
extern
int
cmdlopt_set_help_indents(int optindent, int argindent, int descindent);

/* Sets the name of the application for help/version printing. */
TOE_DLL_API
int
cmdlopt_set_app_name(const char* name);

/* Sets a custom header to show before the options are printed. */
extern
int
cmdlopt_set_help_header(const char* header);

/* Sets a custom footer to show after the options are printed. */
TOE_DLL_API
int
cmdlopt_set_help_footer(const char* footer);


/* Sets the version of the application for version printing. */
TOE_DLL_API
int
cmdlopt_set_app_version(const char* version);

/* Sets the options to parse. */
TOE_DLL_API
int
cmdlopt_set_options(const cmdlopt_opt* options);

#ifdef __cplusplus
}
#endif

#endif /* TOE_CMDLOPT_H */

