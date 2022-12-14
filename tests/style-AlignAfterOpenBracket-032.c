/*
 * AlignAfterOpenBracket: Align
 * IndentWidth: 4
 * UseTab: Never
 */

static err_t
err_set(int enum_value)
{
    return (err_t){
        .value = (enum_value == sfe_system_error) ? -errno : enum_value
    };
}
