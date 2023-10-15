/*
 * AlignAfterOpenBracket: Align
 * AlignOperands: Align
 * BreakBeforeBinaryOperators: NonAssignment
 * ContinuationIndentWidth: 8
 * UseTab: Never
 */

int
main(void)
{
        aaaaaaaaaaaaaa(bb, &(struct new_event){});

        aaaaaaaaaaaaaa(bb, &(struct new_event){
                .flags = 0,
        });
}
