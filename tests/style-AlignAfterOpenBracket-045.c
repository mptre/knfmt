/*
 * AlignAfterOpenBracket: Align
 * AlignOperands: true
 * BreakBeforeBinaryOperators: All
 * ContinuationIndentWidth: 8
 * UseTab: Never
 */

int
main(void)
{
        if (0) {
                if (aaa->bbbbbbbbbbbbbbbbbbbbbb
                    && ccccccccccccccccccccccccccc(
                        aaa, dddddddddddddddddddddddddddddddddddddddddd))
                        return 0;

                if (ccccccccccccccccccccccccccc(
                    aaa, ddddddddddddddddddddddddddddddddddddddddd))
                        return 1;
        }
}
