/* <pre>
 * Copyright 2016 Epiq Solutions, All Rights Reserved
 * </pre>*/

#ifndef __ARG_PARSER_H__
#define __ARG_PARSER_H__

/***** Includes *****/
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/***** Defines *****/
/*****************************************************************************/
/** Macro for easier creation of application_argument structs used to specify
    command line options for the application. Use this macro for command line
    arguments which are required in order for the application to run. The
    argument parser will raise an error if the user does not provide a value
    to a required argument.
    
    @param l_flag String used for double-dash long command line option. 
           Example: "card" for --card
    @param s_flag Character used for single-dash short command line option.
           This should be set to zero if unused.
           Example: 'c' for -c
    @param info Help text for this command line option.
           Example: "The Sidekiq card number"
    @param label Label to be applied to "l_flag" when help is displayed.
           Example: label="NUMBER" => --card=NUMBER
    @param var Address of variable to update with command line value.
    @param var_type One of the types as specified in the variable_type enum.
*/
#define APP_ARG_REQ(l_flag, s_flag, info, label, var, var_type) \
    {                                                           \
        .p_long_flag = l_flag,                                  \
        .short_flag = s_flag,                                   \
        .p_info = info,                                         \
        .p_label = label,                                       \
        .p_var = var,                                           \
        .type = var_type,                                       \
        .required = true,                                       \
        .p_is_set = NULL,                                       \
    }

/*****************************************************************************/
/** Macro for easier creation of application_argument structs used to specify
    command line options for the application. Use this macro for command line
    arguments which are optional in regards to allowing the applictation to 
    run. The argument parser will not do anything if the user does not provide
    these arguments.
    
    @param l_flag String used for double-dash long command line option. 
           Example: "card" for --card
    @param s_flag Character used for single-dash short command line option.
           This should be set to zero if unused.
           Example: 'c' for -c
    @param info Help text for this command line option.
           Example: "The Sidekiq card number"
    @param label Label to be applied to "l_flag" when help is displayed.
           Example: label="NUMBER" => --card=NUMBER
    @param var Address of variable to update with command line value.
    @param var_type One of the types as specified in the variable_type enum.
*/
#define APP_ARG_OPT(l_flag, s_flag, info, label, var, var_type) \
    {                                                           \
        .p_long_flag = l_flag,                                  \
        .short_flag = s_flag,                                   \
        .p_info = info,                                         \
        .p_label = label,                                       \
        .p_var = var,                                           \
        .type = var_type,                                       \
        .required = false,                                      \
        .p_is_set = NULL,                                       \
    }

/*****************************************************************************/
/** Macro for easier creation of application_argument structs used to specify
    command line options for the application. Use this macro for command line
    arguments which are optional (with a presence flag) in regards to allowing
    the application to run. The argument parser will not do anything if the
    user does not provide these arguments.  If the option is specified on the
    command line and @p is_present is non-NULL, then *is_present is assigned
    true, otherwise false.
    
    @param l_flag String used for double-dash long command line option. 
           Example: "card" for --card
    @param s_flag Character used for single-dash short command line option.
           This should be set to zero if unused.
           Example: 'c' for -c
    @param info Help text for this command line option.
           Example: "The Sidekiq card number"
    @param label Label to be applied to "l_flag" when help is displayed.
           Example: label="NUMBER" => --card=NUMBER
    @param var Address of variable to update with command line value.
    @param var_type One of the types as specified in the variable_type enum.
    @param is_present Reference to a bool to indicate presence or absence on the command line.
*/
#define APP_ARG_OPT_PRESENT(l_flag, s_flag, info, label, var, var_type, is_present) \
    {                                                                   \
        .p_long_flag = l_flag,                                          \
        .short_flag = s_flag,                                           \
        .p_info = info,                                                 \
        .p_label = label,                                               \
        .p_var = var,                                                   \
        .type = var_type,                                               \
        .required = false,                                              \
        .p_is_set = is_present,                                         \
    }

/*****************************************************************************/
/** Required macro entry used to indicate the end of an application_argument 
    array passed into arg_parser().
*/
#define APP_ARG_TERMINATOR                      \
    {                                           \
        .p_long_flag = NULL,                    \
        .short_flag = 0,                        \
        .p_info = NULL,                         \
        .p_label = NULL,                        \
        .p_var = NULL,                          \
        .type = BOOL_VAR_TYPE,                  \
        .required = false,                      \
        .p_is_set = NULL,                       \
    }


/***** Enums *****/
/*****************************************************************************/
/** Enumerated type used to assist converting glib argument strings to types
    defined within stdint.h and stbool.h. 
*/
enum variable_type {
    BOOL_VAR_TYPE,
    STRING_VAR_TYPE,
    INT8_VAR_TYPE,
    UINT8_VAR_TYPE,
    INT16_VAR_TYPE,
    UINT16_VAR_TYPE,
    INT32_VAR_TYPE,
    UINT32_VAR_TYPE,
    INT64_VAR_TYPE,
    UINT64_VAR_TYPE,
    FLOAT_VAR_TYPE,
    DOUBLE_VAR_TYPE
};


/***** Structs *****/
/*****************************************************************************/
/** Struct used for specifying command line arguments.
*/
struct application_argument {
    char* p_long_flag;
    char short_flag;
    char* p_info;
    char* p_label;
    void* p_var;
    enum variable_type type;
    bool required;
    bool *p_is_set;
};


/***** Global Function Prototypes *****/
/*****************************************************************************/
/** The arg_parse function is used to abstract some of the code set up
    required to parse command line arguments.

    @param argc Number of arguments received from "main" function.
    @param argv Pointer to arguments received from "main" function.
    @param p_help_short String used for short description of application when
           "-h" or "--help" is specified on the command line.
    @param p_help_long String used for long description of application when
           "-h" or "--help" is specified on the command line.
    @param p_app_arg Array of command line arguments for the application. Note
           that these should be created using either the APP_ARG_OPT() or 
           APP_ARG_REQ() macro with the final element being the 
           APP_ARG_TERMINATOR macro.

    @return int Zero on success, negative value on error. Errno set to one of 
            the following.
                - ENOMEM: Memory allocation error or corruption.
                - ERANGE: Variable value over/underflow.
                - EINVAL: Invalid variable type.
                - ELIBBAD: Bad Glib instance.
*/
extern int32_t arg_parser(
    int argc,
    char** argv,
    const char* p_help_short,
    const char* p_help_long,
    struct application_argument* p_param);

/*****************************************************************************/
/** The arg_parser_print_help function can be called at any point where the
    application needs to print the help text and exit. This can be useful in
    cases where a variable check on a command line argument outside of 
    arg_parser() failed and displaying the help text would clarify the user's
    mistake. This function will call exit() upon completion.

    @param p_app_name String representation of the application's name. This
           argument should be populated with something akin to argv[0].
    @param p_help_short String used for short description of application when
           "-h" or "--help" is specified on the command line.
    @param p_help_long String used for long description of application when
           "-h" or "--help" is specified on the command line.
    @param p_app_arg Array of command line arguments for the application. Note
           that these should be created using either the APP_ARG_OPT() or 
           APP_ARG_REQ() macro with the final element being the 
           APP_ARG_TERMINATOR macro. 

    @return void
*/
extern void arg_parser_print_help(
    const char* p_app_name,
    const char* p_help_short,
    const char* p_help_long,
    struct application_argument* p_param);

#ifdef __cplusplus
}
#endif

#endif
