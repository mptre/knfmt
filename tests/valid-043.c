/*
 * Function pointer argument.
 */

void
malloc_printit(
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__, 1, 2))))
{
}
