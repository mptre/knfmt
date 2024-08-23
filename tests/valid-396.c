/*
 * Attributes before declaration initialization.
 */

union fuzzer_callback fuzzer_callback_init_default
		      __attribute__((used))
		      __attribute__((FUZZER_SECTION_INIT))
		      = {0};
