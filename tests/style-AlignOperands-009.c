/*
 * AlignOperands: Align
 * ContinuationIndentWidth: 8
 */

int
main(void)
{
	return (struct addr){
		.paddr = addr >> 8,
		.vaddr = addr & 0xff,
	};
}
