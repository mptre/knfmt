/*
 * Token use-after-free caused by branches covering the same tokens.
 */

#ifndef b1111

#ifdef b2222
int debug = 1;
#endif

0

#endif
