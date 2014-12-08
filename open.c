/*
 * ProFTPD - mod_erase open
 * Copyright (c) 2014 TJ Saunders
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 * As a special exemption, TJ Saunders and other respective copyright holders
 * give permission to link this program with OpenSSL, and distribute the
 * resulting executable, without including the source code for OpenSSL in the
 * source distribution.
 */

#include "mod_erase.h"
#include "open.h"

static const char *stroflags(pool *p, int flags) {
  char *str = "";

  if (flags & O_WRONLY) {
    str = pstrcat(p, str, "O_WRONLY", NULL);
  }

#if defined(O_DIRECT)
  if (flags & O_DIRECT) {
    str = pstrcat(p, str, *str ? "|" : "", "O_DIRECT", NULL);
  }
#endif

#if defined(O_SYNC)
  if (flags & O_SYNC) {
    str = pstrcat(p, str, *str ? "|" : "", "O_SYNC", NULL);
  }
#endif

  return str;
}

int erase_open(pool *p, const char *path, struct stat *st) {
  int fd, flags, res;

  if (p == NULL ||
      path == NULL) {
    errno = EINVAL;
    return -1;
  }

  flags = O_WRONLY;

  /* Note: should use of these flags be controllable via EraseOptions? */
#if defined(O_DIRECT)
  flags |= O_DIRECT;
#endif

#if defined(O_SYNC)
  flags |= O_SYNC;
#endif

  fd = open(path, flags);
  if (fd < 0) {
    int xerrno = errno;

    pr_trace_msg(erase_channel, 2,
      "error opening '%s' using %s: %s", path,
      stroflags(p, flags), strerror(xerrno));

    errno = xerrno;
    return -1;
  }

  res = fstat(fd, st);
  if (res < 0) {
    int xerrno = errno;

    (void) close(fd);
    pr_trace_msg(erase_channel, 3,
      "fstat(2) error on '%s' (fd %d): %s", path, fd, strerror(xerrno));

    errno = xerrno;
    return -1;
  }

  if (!S_ISREG(st->st_mode)) {
    (void) close(fd);
    pr_trace_msg(erase_channel, 3,
      "unable to use '%s': Not a regular file", path);
    
    errno = EPERM;
    return -1;
  }

#if defined(F_NOCACHE)
  res = fcntl(fd, F_NOCACHE, 1);
  if (res < 0) {
    pr_trace_msg(erase_channel, 7,
      "error setting F_NOCACHE on fd %d: %s", fd, strerror(errno));
  }
#endif

  return fd;
}
