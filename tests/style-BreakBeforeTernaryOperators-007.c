/*
 * AlignOperands: Align
 * BreakBeforeTernaryOperators: true
 * ContinuationIndentWidth: 8
 * UseTab: Never
 */

int
main(void)
{
        log_error(1,
                "str",
                is_true ? "true" : "false",
                a, b);
}
