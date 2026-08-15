/*******************************************************************
 * File:        os_posix
 * Purpose:     POSIX implementation of the oras "os" veneer
 * Author:      Gerph
 ******************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include "kernel.h"
#include "os.h"

static _kernel_oserror posix_readline_error;

/*************************************************** Gerph *********
 Function:      oras_os_file_info
 Description:   Determine the filing system object type of a path, and
                (where the caller wants it) RISC OS load/exec/attr
                metadata
 Parameters:    path-> the path to examine
                object_type-> where to store the object type (0 = not
                              found, 1 = file, 2 = directory)
                loadaddr-> where to store the load address, or NULL if
                           the caller only wants the object type
                execaddr-> where to store the exec address
                attrs-> where to store the attributes
 Returns:       1 if successful, 0 if failed
 Note:          POSIX files never carry RISC OS load/exec/attr
                metadata, so a request for that metadata (loadaddr
                non-NULL) always fails; riscos_metadata_read() then
                falls back to deriving the filetype from a ",xyz"
                filename suffix. A plain object-type query (loadaddr
                NULL, used to test whether a path exists and whether
                it is a file or directory) is answered from stat().
 ******************************************************************/
int oras_os_file_info(const char *path, int *object_type, uint32_t *loadaddr,
                      uint32_t *execaddr, uint32_t *attrs)
{
    struct stat st;

    (void)execaddr;
    (void)attrs;
    if (loadaddr != NULL)
    {
        return 0;
    }
    if (stat(path, &st) != 0)
    {
        *object_type = 0;
        return 1;
    }
    *object_type = S_ISDIR(st.st_mode) ? 2 : 1;
    return 1;
}

/*************************************************** Gerph *********
 Function:      oras_os_file_set_info
 Description:   Apply RISC OS load/exec/attr metadata to a file
 Parameters:    path-> the file to update
                loadaddr = the load address to apply
                execaddr = the exec address to apply
                attrs = the attributes to apply
 Returns:       1 if successful, 0 if failed
 Note:          POSIX files have no RISC OS load/exec/attr metadata to
                apply; this is a no-op that reports success so that
                riscos_metadata_apply() does not abort a pull merely
                because the destination cannot represent it.
 ******************************************************************/
int oras_os_file_set_info(const char *path, uint32_t loadaddr, uint32_t execaddr,
                          uint32_t attrs)
{
    (void)path;
    (void)loadaddr;
    (void)execaddr;
    (void)attrs;
    return 1;
}

/*************************************************** Gerph *********
 Function:      oras_os_file_set_type
 Description:   Apply a RISC OS filetype to a file
 Parameters:    path-> the file to update
                filetype = the filetype to apply
 Returns:       1 if successful, 0 if failed
 Note:          As oras_os_file_set_info(), this is a no-op on POSIX.
 ******************************************************************/
int oras_os_file_set_type(const char *path, uint32_t filetype)
{
    (void)path;
    (void)filetype;
    return 1;
}

/*************************************************** Gerph *********
 Function:      oras_os_file_create_directory
 Description:   Create a directory, tolerating one that already exists
 Parameters:    path-> the directory to create
 Returns:       1 if successful, 0 if failed
 ******************************************************************/
int oras_os_file_create_directory(const char *path)
{
    if (mkdir(path, 0777) == 0)
    {
        return 1;
    }
    return errno == EEXIST;
}

/*************************************************** Gerph *********
 Function:      os_readline_secret
 Description:   Read a line without echoing the entered characters
 Parameters:    line-> line to read
                len = max length including terminator
 Returns:       pointer to error, or NULL if no error
 ******************************************************************/
_kernel_oserror *os_readline_secret(char *line, int len)
{
    struct termios original;
    struct termios masked;
    int have_termios;
    size_t got;

    have_termios = tcgetattr(STDIN_FILENO, &original) == 0;
    if (have_termios)
    {
        masked = original;
        masked.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &masked);
    }
    if (fgets(line, len, stdin) == NULL)
    {
        line[0] = '\0';
    }
    if (have_termios)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    }
    printf("\n");
    got = strlen(line);
    if (got > 0 && line[got - 1] == '\n')
    {
        line[got - 1] = '\0';
    }
    if (ferror(stdin))
    {
        strcpy(posix_readline_error.errmess, "Unable to read from standard input");
        posix_readline_error.errnum = 0;
        return &posix_readline_error;
    }
    return NULL;
}
