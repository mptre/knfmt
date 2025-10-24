/*
 * ColumnLimit: 100
 */

#define E(elf, field)							\
	((elf)->cls == ELFCLASS64 ?					\
	 (elf)->ptr.c64->field : (elf)->ptr.c32->field)
