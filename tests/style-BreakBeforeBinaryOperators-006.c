/*
 * AlignAfterOpenBracket: Align
 * BreakBeforeBinaryOperators: NonAssignment
 * ContinuationIndentWidth: 8
 * UseTab: Never
 */

int
main(void)
{
        return ((aaa->aaaa == aaaaaaaaaaaaaaaaa && !aaaaaaa)
                // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
                // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
                // CCCCCCCCCCCCCCCCCCCCCCCCCCC
                || (aaa->aaaaaa.aaaa && aaa->aaaa == aaaaaaaaaaaaaaaaaaaaaaa
                    && aaaaaaa && !aaa->aaaaaaaaaaaa && !aaa->a.aaaaaaa &&
                    !aaaaaaaaaaaaaaaaaaaaaaaaa(&aaa->a))
                /* CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC */
                || (aaa->aaaaaaaaaaa == aaaaaaaaaaaaaaaa && !aaaaaaa)
                /* CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC */
                || (aaa->aaaaaaaaaaa == aaaaaaaaaaaaaaaa &&
                    aaa->aaaa == aaaaaaaaaaaaaaaaaaaaaaa && aaaaaaa)
                || (aaa->aaaaaa.aaaa && aaa->aaaa == aaaaaaaaaaaaaaaaaaaaaaa &&
                    aaaaaaa && aaaaaaaaaaaaaaaaaaaaaaaaa(&aaa->a) && !aaa->a.aaaaaaa)
                || (aaaaaaaaaaaa && !aaaaaaa &&
                    (aaa->aaaaaaaaaaa != aaaaaaaaaaaaaaaa) &&
                    (aaa->aaaa == aaaaaaaaaaaaaaaaa || aaaaaa)));
}
