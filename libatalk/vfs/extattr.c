/*
   Unix SMB/CIFS implementation.
   Samba system utilities
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison  1998-2005
   Copyright (C) Timur Bakeyev        2005
   Copyright (C) Bjoern Jacke    2006-2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   sys_copyxattr modified from LGPL2.1 libattr copyright
   Copyright (C) 2001-2002 Silicon Graphics, Inc.  All Rights Reserved.
   Copyright (C) 2001 Andreas Gruenbacher.

   Samba 3.0.28, modified for netatalk.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#elif HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_EA_H
#include <sys/ea.h>
#endif

#ifdef SOLARIS

#include <dirent.h>
#endif

#ifdef HAVE_SYS_EXTATTR_H
#include <sys/extattr.h>
#endif

#include <atalk/adouble.h>
#include <atalk/compat.h>
#include <atalk/ea.h>
#include <atalk/errchk.h>
#include <atalk/logger.h>
#include <atalk/util.h>

/******** Solaris EA helper function prototypes ********/
#ifdef SOLARIS
#define SOLARIS_ATTRMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH
static int solaris_write_xattr(int attrfd, const char *value, size_t size);
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size);
static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size);
static int solaris_unlinkat(int attrdirfd, const char *name);
static int solaris_attropen(const char *path, const char *attrpath, int oflag,
                            mode_t mode);
static int solaris_attropenat(int filedes, const char *path,
                              const char *attrpath, int oflag, mode_t mode);
static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode);
#endif

/**************************************************************************
 Wrappers for extented attribute calls. Based on the Linux package.
****************************************************************************/
static char attr_name[256 + 5] = "user.";

static const char *prefix(const char *uname)
{
#if defined(SOLARIS) || defined(__APPLE__)
    return uname;
#else
    strlcpy(attr_name + 5, uname, 256);
    return attr_name;
#endif
}

int sys_getxattrfd(int fd _U_, const char *uname _U_, int oflag _U_, ...)
{
#if defined(SOLARIS)
    int eafd;
    va_list args;
    mode_t mode = 0;

    if (oflag & O_CREAT) {
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    if (oflag & O_CREAT) {
        eafd = solaris_openat(fd, uname, oflag | O_XATTR, mode);
    } else {
        eafd = solaris_openat(fd, uname, oflag | O_XATTR, mode);
    }

    return eafd;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_getxattr(const char *path, const char *uname, void *value,
                     size_t size)
{
    const char *name = prefix(uname);
#if defined(HAVE_GETXATTR)
#ifndef XATTR_ADD_OPT
    return getxattr(path, name, value, size);
#else
    int options = 0;
#ifdef __APPLE__

    /* macOS only returns EA size if value is NULL */
    if (size == 0) {
        value = NULL;
    }

#endif
    return getxattr(path, name, value, size, 0, options);
#endif
#elif defined(HAVE_GETEA)
    return getea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FILE)
    ssize_t retval;

    /*
     * The BSD implementation has a nasty habit of silently truncating
     * the returned value to the size of the buffer, so we have to check
     * that the buffer is large enough to fit the returned value.
     */
    if ((retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, NULL,
                                   0)) >= 0) {
        if (size == 0)
            /* size == 0 means only return size */
        {
            return retval;
        }

        if (retval > size) {
            errno = ERANGE;
            return -1;
        }

        if ((retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, value,
                                       size)) >= 0) {
            return retval;
        }
    }

    LOG(log_maxdebug, logtype_default,
        "sys_getxattr: extattr_get_file() failed with: %s\n", strerror(errno));
    return -1;
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrfd = solaris_attropen(path, name, O_RDONLY, 0);

    if (attrfd >= 0) {
        ret = solaris_read_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_fgetxattr(int filedes, const char *uname, void *value, size_t size)
{
    const char *name = prefix(uname);
#if defined(HAVE_FGETXATTR)
#ifndef XATTR_ADD_OPT
    return fgetxattr(filedes, name, value, size);
#else
    int options = 0;
#ifdef __APPLE__

    /* macOS only returns EA size if value is NULL */
    if (size == 0) {
        value = NULL;
    }

#endif
    return fgetxattr(filedes, name, value, size, 0, options);
#endif
#elif defined(HAVE_FGETEA)
    return fgetea(filedes, name, value, size);
#elif defined(HAVE_EXTATTR_GET_FD)
    char *s;
    ssize_t retval;
    int attrnamespace = (strncmp(name, "system", 6) == 0) ?
                        EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
    const char *attrname = ((s = strchr(name, '.')) == NULL) ? name : s + 1;

    if ((retval = extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0)) >= 0) {
        if (size == 0) {
            /* size == 0 means only return size */
            return retval;
        }

        if (retval > size) {
            errno = ERANGE;
            return -1;
        }

        if ((retval = extattr_get_fd(filedes, attrnamespace, attrname, value,
                                     size)) >= 0) {
            return retval;
        }
    }

    LOG(log_debug, logtype_default, "sys_fgetxattr: extattr_get_fd(): %s",
        strerror(errno));
    return -1;
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrfd = solaris_openat(filedes, name, O_RDONLY | O_XATTR, 0);

    if (attrfd >= 0) {
        ret = solaris_read_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_lgetxattr(const char *path, const char *uname, void *value,
                      size_t size)
{
    const char *name = prefix(uname);
#if defined(HAVE_LGETXATTR)
    return lgetxattr(path, name, value, size);
#elif defined(HAVE_GETXATTR) && defined(XATTR_ADD_OPT)
    int options = XATTR_NOFOLLOW;
#ifdef __APPLE__

    /* macOS only returns EA size if value is NULL */
    if (size == 0) {
        value = NULL;
    }

#endif
    return getxattr(path, name, value, size, 0, options);
#elif defined(HAVE_LGETEA)
    return lgetea(path, name, value, size);
#elif defined(HAVE_EXTATTR_GET_LINK)
    ssize_t retval;
    retval = extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0);

    if (retval == -1) {
        LOG(log_maxdebug, logtype_default, "extattr_get_link(): %s",
            strerror(errno));
        return -1;
    }

    if (size == 0)
        /* Only interested in size of xattr */
    {
        return retval;
    }

    if (retval > size) {
        errno = ERANGE;
        return -1;
    }

    return extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, value, size);
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrfd = solaris_attropen(path, name, O_RDONLY | O_NOFOLLOW, 0);

    if (attrfd >= 0) {
        ret = solaris_read_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

#if defined(HAVE_EXTATTR_LIST_FILE)

#define EXTATTR_PREFIX(s)	(s), (sizeof((s))-1)

static struct {
    int space;
    const char *name;
    size_t len;
}
extattr[] = {
    { EXTATTR_NAMESPACE_SYSTEM, EXTATTR_PREFIX("") },
    { EXTATTR_NAMESPACE_USER, EXTATTR_PREFIX("") },
};

typedef union {
    const char *path;
    int filedes;
} extattr_arg;

static ssize_t bsd_attr_list(int type, extattr_arg arg, char *list, size_t size)
{
    ssize_t list_size;
    int i, len;

    switch (type) {
#if defined(HAVE_EXTATTR_LIST_FILE)

    case 0:
        list_size = extattr_list_file(arg.path, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif
#if defined(HAVE_EXTATTR_LIST_LINK)

    case 1:
        list_size = extattr_list_link(arg.path, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif
#if defined(HAVE_EXTATTR_LIST_FD)

    case 2:
        list_size = extattr_list_fd(arg.filedes, EXTATTR_NAMESPACE_USER, list, size);
        break;
#endif

    default:
        errno = ENOSYS;
        return -1;
    }

    /* Some error happend. Errno should be set by the previous call */
    if (list_size < 0) {
        return -1;
    }

    /* No attributes */
    if (list_size == 0) {
        return 0;
    }

    /* XXX: Call with an empty buffer may be used to calculate
       necessary buffer size. Unfortunately, we can't say, how
       many attributes were returned, so here is the potential
       problem with the emulation.
    */
    if (list == NULL) {
        return list_size;
    }

    /* Buffer is too small to fit the results */
    if (list_size > size) {
        errno = ERANGE;
        return -1;
    }

    /* Convert from pascal strings to C strings */
    len = (unsigned char)list[0];
    memmove(list, list + 1, list_size - 1);

    for (i = len; i < list_size;) {
        LOG(log_maxdebug, logtype_afpd, "len: %d, i: %d", len, i);
        len = (unsigned char)list[i];
        list[i] = '\0';
        i += len + 1;
    }

    return list_size;
}

#endif

#if defined(HAVE_LISTXATTR)
static ssize_t remove_user(ssize_t ret, char *list, size_t size)
{
    size_t len;
    char *ptr;
    char *ptr1;
    ssize_t ptrsize;
#ifdef __APPLE__
    /* macOS doesn't prefix EAs with "user." */
    return ret;
#else

    if (ret <= 0 || size == 0) {
        return ret;
    }

    ptrsize = ret;
    ptr = ptr1 = list;

    while (ptrsize > 0) {
        len = strlen(ptr1) + 1;
        ptrsize -= len;

        if (strncmp(ptr1, "user.", 5)) {
            ptr1 += len;
            continue;
        }

        memmove(ptr, ptr1 + 5, len - 5);
        ptr += len - 5;
        ptr1 += len;
    }

    return ptr - list;
#endif
}
#endif

ssize_t sys_listxattr(const char *path, char *list, size_t size)
{
#if defined(HAVE_LISTXATTR)
    ssize_t ret;
#ifndef XATTR_ADD_OPT
    ret = listxattr(path, list, size);
#else
    int options = 0;
    ret = listxattr(path, list, size, options);
#endif
    return remove_user(ret, list, size);
#elif defined(HAVE_LISTEA)
    return listea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_FILE)
    extattr_arg arg;
    arg.path = path;
    return bsd_attr_list(0, arg, list, size);
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);

    if (attrdirfd >= 0) {
        ret = solaris_list_xattr(attrdirfd, list, size);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_flistxattr(int filedes _U_, const char *path, char *list,
                       size_t size)
{
#if defined(HAVE_LISTXATTR)
    ssize_t ret;
#ifndef XATTR_ADD_OPT
    ret = listxattr(path, list, size);
#else
    int options = 0;
    ret = listxattr(path, list, size, options);
#endif
    return remove_user(ret, list, size);
#elif defined(HAVE_LISTEA)
    return listea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_FILE)
    extattr_arg arg;
    arg.path = path;
    return bsd_attr_list(0, arg, list, size);
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrdirfd = solaris_attropenat(filedes, path, ".", O_RDONLY, 0);

    if (attrdirfd >= 0) {
        ret = solaris_list_xattr(attrdirfd, list, size);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

ssize_t sys_llistxattr(const char *path, char *list, size_t size)
{
#if defined(HAVE_LLISTXATTR)
    ssize_t ret;
    ret = llistxattr(path, list, size);
    return remove_user(ret, list, size);
#elif defined(HAVE_LISTXATTR) && defined(XATTR_ADD_OPT)
    ssize_t ret;
    int options = XATTR_NOFOLLOW;
    ret = listxattr(path, list, size, options);
    return remove_user(ret, list, size);
#elif defined(HAVE_LLISTEA)
    return llistea(path, list, size);
#elif defined(HAVE_EXTATTR_LIST_LINK)
    extattr_arg arg;
    arg.path = path;
    return bsd_attr_list(1, arg, list, size);
#elif defined(SOLARIS)
    ssize_t ret = -1;
    int attrdirfd = solaris_attropen(path, ".", O_RDONLY | O_NOFOLLOW, 0);

    if (attrdirfd >= 0) {
        ret = solaris_list_xattr(attrdirfd, list, size);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_removexattr(const char *path, const char *uname)
{
    const char *name = prefix(uname);
#if defined(HAVE_REMOVEXATTR)
#ifndef XATTR_ADD_OPT
    return removexattr(path, name);
#else
    int options = 0;
    return removexattr(path, name, options);
#endif
#elif defined(HAVE_REMOVEEA)
    return removeea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_FILE)
    return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, uname);
#elif defined(SOLARIS)
    int ret = -1;
    int attrdirfd = solaris_attropen(path, ".", O_RDONLY, 0);

    if (attrdirfd >= 0) {
        ret = solaris_unlinkat(attrdirfd, name);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_fremovexattr(int filedes _U_, const char *path, const char *uname)
{
    const char *name = prefix(uname);
#if defined(HAVE_REMOVEXATTR)
#ifndef XATTR_ADD_OPT
    return removexattr(path, name);
#else
    int options = 0;
    return removexattr(path, name, options);
#endif
#elif defined(HAVE_REMOVEEA)
    return removeea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_FILE)
    return extattr_delete_file(path, EXTATTR_NAMESPACE_USER, uname);
#elif defined(SOLARIS)
    int ret = -1;
    int attrdirfd = solaris_attropenat(filedes, path, ".", O_RDONLY, 0);

    if (attrdirfd >= 0) {
        ret = solaris_unlinkat(attrdirfd, name);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_lremovexattr(const char *path, const char *uname)
{
    const char *name = prefix(uname);
#if defined(HAVE_LREMOVEXATTR)
    return lremovexattr(path, name);
#elif defined(HAVE_REMOVEXATTR) && defined(XATTR_ADD_OPT)
    int options = XATTR_NOFOLLOW;
    return removexattr(path, name, options);
#elif defined(HAVE_LREMOVEEA)
    return lremoveea(path, name);
#elif defined(HAVE_EXTATTR_DELETE_LINK)
    return extattr_delete_link(path, EXTATTR_NAMESPACE_USER, uname);
#elif defined(SOLARIS)
    int ret = -1;
    int attrdirfd = solaris_attropen(path, ".", O_RDONLY | O_NOFOLLOW, 0);

    if (attrdirfd >= 0) {
        ret = solaris_unlinkat(attrdirfd, name);
        close(attrdirfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_setxattr(const char *path, const char *uname, const void *value,
                 size_t size, int flags _U_)
{
    const char *name = prefix(uname);
#if defined(HAVE_SETXATTR)
#ifndef XATTR_ADD_OPT
    return setxattr(path, name, value, size, flags);
#else
    return setxattr(path, name, value, size, 0, flags);
#endif
#elif defined(HAVE_SETEA)
    return setea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FILE)
    int retval = 0;

    if (flags) {
        /* Check attribute existence */
        retval = extattr_get_file(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0);

        if (retval < 0) {
            /* REPLACE attribute, that doesn't exist */
            if (flags & XATTR_REPLACE && errno == ENOATTR) {
                errno = ENOATTR;
                return -1;
            }

            /* Ignore other errors */
        } else {
            /* CREATE attribute, that already exists */
            if (flags & XATTR_CREATE) {
                errno = EEXIST;
                return -1;
            }
        }
    }

    retval = extattr_set_file(path, EXTATTR_NAMESPACE_USER, uname, value, size);
    return (retval < 0) ? -1 : 0;
#elif defined(SOLARIS)
    int ret = -1;
    int myflags = O_RDWR;
    int attrfd;

    if (flags & XATTR_CREATE) {
        myflags |= O_EXCL;
    }

    if (!(flags & XATTR_REPLACE)) {
        myflags |= O_CREAT;
    }

    attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);

    if (attrfd >= 0) {
        ret = solaris_write_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_fsetxattr(int filedes, const char *uname, const void *value,
                  size_t size, int flags _U_)
{
    const char *name = prefix(uname);
#if defined(HAVE_FSETXATTR)
#ifndef XATTR_ADD_OPT
    return fsetxattr(filedes, name, value, size, flags);
#else
    return fsetxattr(filedes, name, value, size, 0, flags);
#endif
#elif defined(HAVE_FSETEA)
    return fsetea(filedes, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_FD)
    char *s;
    int retval = 0;
    int attrnamespace = (strncmp(name, "system", 6) == 0) ?
                        EXTATTR_NAMESPACE_SYSTEM : EXTATTR_NAMESPACE_USER;
    const char *attrname = ((s = strchr(name, '.')) == NULL) ? name : s + 1;

    if (flags) {
        /* Check attribute existence */
        retval = extattr_get_fd(filedes, attrnamespace, attrname, NULL, 0);

        if (retval < 0) {
            /* REPLACE attribute, that doesn't exist */
            if (flags & XATTR_REPLACE && errno == ENOATTR) {
                errno = ENOATTR;
                return -1;
            }

            /* Ignore other errors */
        } else {
            if (flags & XATTR_CREATE) {
                errno = EEXIST;
                return -1;
            }
        }
    }

    retval = extattr_set_fd(filedes, attrnamespace, attrname, value, size);
    return (retval < 0) ? -1 : 0;
#elif defined(SOLARIS)
    int ret = -1;
    int myflags = O_RDWR | O_XATTR;
    int attrfd;

    if (flags & XATTR_CREATE) {
        myflags |= O_EXCL;
    }

    if (!(flags & XATTR_REPLACE)) {
        myflags |= O_CREAT;
    }

    attrfd = solaris_openat(filedes, name, myflags, (mode_t) SOLARIS_ATTRMODE);

    if (attrfd >= 0) {
        ret = solaris_write_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

int sys_lsetxattr(const char *path, const char *uname, const void *value,
                  size_t size, int flags _U_)
{
    const char *name = prefix(uname);
#if defined(HAVE_LSETXATTR)
    return lsetxattr(path, name, value, size, flags);
#elif defined(HAVE_SETXATTR) && defined(XATTR_ADD_OPT)
    flags |= XATTR_NOFOLLOW;
    return setxattr(path, name, value, size, 0, flags);
#elif defined(LSETEA)
    return lsetea(path, name, value, size, flags);
#elif defined(HAVE_EXTATTR_SET_LINK)
    int retval = 0;

    if (flags) {
        /* Check attribute existence */
        retval = extattr_get_link(path, EXTATTR_NAMESPACE_USER, uname, NULL, 0);

        if (retval < 0) {
            /* REPLACE attribute, that doesn't exist */
            if (flags & XATTR_REPLACE && errno == ENOATTR) {
                errno = ENOATTR;
                return -1;
            }

            /* Ignore other errors */
        } else {
            /* CREATE attribute, that already exists */
            if (flags & XATTR_CREATE) {
                errno = EEXIST;
                return -1;
            }
        }
    }

    retval = extattr_set_link(path, EXTATTR_NAMESPACE_USER, uname, value, size);
    return (retval < 0) ? -1 : 0;
#elif defined(SOLARIS)
    int ret = -1;
    int myflags = O_RDWR | O_NOFOLLOW;
    int attrfd;

    if (flags & XATTR_CREATE) {
        myflags |= O_EXCL;
    }

    if (!(flags & XATTR_REPLACE)) {
        myflags |= O_CREAT;
    }

    attrfd = solaris_attropen(path, name, myflags, (mode_t) SOLARIS_ATTRMODE);

    if (attrfd >= 0) {
        ret = solaris_write_xattr(attrfd, value, size);
        close(attrfd);
    }

    return ret;
#else
    errno = ENOSYS;
    return -1;
#endif
}

/**************************************************************************
 helper functions for Solaris' EA support
****************************************************************************/
#ifdef SOLARIS
static ssize_t solaris_read_xattr(int attrfd, void *value, size_t size)
{
    struct stat sbuf;

    if (fstat(attrfd, &sbuf) == -1) {
        return -1;
    }

    /* This is to return the current size of the named extended attribute */
    if (size == 0) {
        return sbuf.st_size;
    }

    /* check size and read xattr */
    if (sbuf.st_size > size) {
        return -1;
    }

    return read(attrfd, value, sbuf.st_size);
}

static ssize_t solaris_list_xattr(int attrdirfd, char *list, size_t size)
{
    ssize_t len = 0;
    DIR *dirp;
    struct dirent *de;
    int newfd = dup(attrdirfd);
    /* CAUTION: The originating file descriptor should not be
                used again following the call to fdopendir().
                For that reason we dup() the file descriptor
    	        here to make things more clear. */
    dirp = fdopendir(newfd);

    while ((de = readdir(dirp))) {
        size_t listlen;

        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
            /* we don't want "." and ".." here: */
            LOG(log_maxdebug, logtype_default, "skipped EA %s\n", de->d_name);
            continue;
        }

        listlen = strlen(de->d_name);

        if (size == 0) {
            /* return the current size of the list of extended attribute names*/
            len += listlen + 1;
        } else {
            /* check size and copy entry + nul into list. */
            if ((len + listlen + 1) > size) {
                errno = ERANGE;
                len = -1;
                break;
            } else {
                strcpy(list + len, de->d_name);
                len += listlen;
                list[len] = '\0';
                ++len;
            }
        }
    }

    if (closedir(dirp) == -1) {
        LOG(log_error, logtype_default, "closedir dirp: %s", strerror(errno));
        return -1;
    }

    return len;
}

static int solaris_unlinkat(int attrdirfd, const char *name)
{
    if (unlinkat(attrdirfd, name, 0) == -1) {
        return -1;
    }

    return 0;
}

static int solaris_attropen(const char *path, const char *attrpath, int oflag,
                            mode_t mode)
{
    EC_INIT;
    int filedes = -1, eafd = -1;

    if ((filedes = open(path, O_RDONLY | (oflag & O_NOFOLLOW), mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
        case OPEN_NOFOLLOW_ERRNO:
            EC_FAIL;

        default:
            LOG(log_debug, logtype_default, "open(\"%s\"): %s", fullpathname(path),
                strerror(errno));
            EC_FAIL;
        }
    }

    if ((eafd = openat(filedes, attrpath, oflag | O_XATTR, mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
            EC_FAIL;

        default:
            LOG(log_debug, logtype_default, "openat(\"%s\"): %s", fullpathname(path),
                strerror(errno));
            EC_FAIL;
        }
    }

EC_CLEANUP:

    if (filedes != -1) {
        close(filedes);
    }

    if (ret != 0) {
        if (eafd != -1) {
            close(eafd);
        }

        eafd = -1;
    }

    return eafd;
}

static int solaris_attropenat(int filedes, const char *path,
                              const char *attrpath, int oflag, mode_t mode)
{
    EC_INIT;
    int eafd = -1;

    if ((eafd = openat(filedes, attrpath, oflag | O_XATTR, mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
            EC_FAIL;

        default:
            LOG(log_debug, logtype_default, "openat(\"%s\"): %s", fullpathname(path),
                strerror(errno));
            EC_FAIL;
        }
    }

EC_CLEANUP:

    if (ret != 0) {
        if (eafd != -1) {
            close(eafd);
        }

        eafd = -1;
    }

    return eafd;
}


static int solaris_openat(int fildes, const char *path, int oflag, mode_t mode)
{
    int filedes;

    if ((filedes = openat(fildes, path, oflag, mode)) == -1) {
        switch (errno) {
        case ENOENT:
        case EEXIST:
        case EACCES:
            break;

        default:
            LOG(log_debug, logtype_default, "openat(\"%s\"): %s",
                path, strerror(errno));
            errno = ENOATTR;
            break;
        }
    }

    return filedes;
}

static int solaris_write_xattr(int attrfd, const char *value, size_t size)
{
    if ((ftruncate(attrfd, 0) == 0) && (write(attrfd, value, size) == size)) {
        return 0;
    } else {
        LOG(log_error, logtype_default, "solaris_write_xattr: %s",
            strerror(errno));
        return -1;
    }
}

#endif /*SOLARIS*/

