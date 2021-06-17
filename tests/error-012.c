#ifndef __LIBPAYLOAD__

/* loops per microsecond */
static unsigned long micro = 1;

__attribute__ ((noinline)) void myusec_delay(int usecs)
{
}

#else

int x;

#endif
