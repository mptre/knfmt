/*
 * Never break struct field access.
 */

int
main(void)
{
	x = ASN1_INTEGER_get(pci->
	      pcPathLengthConstraint);
	x = ASN1_INTEGER_get(pci.
	      pcPathLengthConstraint);
}
