/*
 * Function pointer type consisting of unknown identifiers.
 */

typedef
EFI_STATUS
(EFIAPI *EFI_RNG_GET_INFO) (
    IN struct _EFI_RNG_PROTOCOL		*This,
    IN  OUT UINTN			*RNGAlgorithmListSize,
    OUT EFI_RNG_ALGORITHM		*RNGAlgorithmList
    );
