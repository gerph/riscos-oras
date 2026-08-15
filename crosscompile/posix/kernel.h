/*******************************************************************
 * File:        kernel
 * Purpose:     Minimal RISC OS kernel.h shim for the POSIX crosscompile
 *              build. oras.h's os.h veneer declares functions in terms
 *              of _kernel_oserror; only the type is needed here since
 *              the POSIX implementations never issue real SWI calls.
 * Author:      Gerph
 ******************************************************************/

#ifndef KERNEL_H
#define KERNEL_H

typedef struct _kernel_oserror
{
    int errnum;
    char errmess[252];
} _kernel_oserror;

#endif
