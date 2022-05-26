/*
 * Moving passed branch in lexer_if_flags() bug.
 */

/* $OpenBSD: aes_core.c,v 1.13 2015/11/05 21:59:13 miod Exp $ */

#ifndef AES_ASM

int x;

int
main(void)
{
#ifdef FULL_UNROLL
	t0 = 0;
#else
	t1 = 1;
#endif
}

#else /* AES_ASM */

static const u8 Te4;

#endif /* AES_ASM */
