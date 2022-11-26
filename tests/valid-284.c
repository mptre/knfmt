/*
 * Preprocessor directives part of declaration.
 */

ASN1_SEQUENCE(ASProviderAttestation) = {
	ASN1_EXP_OPT(ASProviderAttestation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(ASProviderAttestation, customerASID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ASProviderAttestation, providers, ProviderAS),
} ASN1_SEQUENCE_END(ASProviderAttestation);
