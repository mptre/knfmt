/*
 * Sense cpp macro alignment.
 */

#define FOR_REGISTERS(OP)						\
	/*  0 */ OP(A,   al,   al,   ax,   eax,  rax, xmm0,   ymm0,   zmm0)\
	/*  1 */ OP(C,   cl,   cl,   cx,   ecx,  rcx, xmm1,   ymm1,   zmm1)\
	/*  2 */ OP(D,   dl,   dl,   dx,   edx,  rdx, xmm2,   ymm2,   zmm2)
