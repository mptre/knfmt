/*
 * Leave indentation before line continuation in cpp macro intact when there's
 * nothing to align.
 */

#define VMX_EXIT_INFO_COMPLETE				\
    (VMX_EXIT_INFO_HAVE_RIP | VMX_EXIT_INFO_HAVE_REASON)
