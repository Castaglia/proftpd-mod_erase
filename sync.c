/*
 * ProFTPD - mod_erase sync
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
#include "sync.h"

int erase_dsync(pool *p, const char *dir_path, int file_fd) {
  int res;

#if defined(HAVE_DIRFD)
  int dir_fd;
  DIR *dirh;

  dirh = opendir(dir_path);
  if (dirh == NULL) {
    int xerrno = errno;

    pr_trace_msg(erase_channel, 3,
      "unable to open directory '%s' for dsync: %s", dir_path,
      strerror(xerrno)); 

    errno = xerrno;
    return -1;
  }

  dir_fd = dirfd(dirh);
  if (dir_fd < 0) {
    int xerrno = errno;

    (void) closedir(dirh);

    pr_trace_msg(erase_channel, 3,
      "unable to get fd for directory '%s' for dsync: %s", dir_path,
      strerror(xerrno)); 

    errno = xerrno;
    return -1;
  }

  if (erase_fsync(p, dir_fd) < 0) {
    pr_trace_msg(erase_channel, 9,
      "fsync error on directory '%s' (fd %d): %s", dir_path, dir_fd,
      strerror(errno));
  }

  if (closedir(dirh) < 0) {
    int xerrno = errno;

    pr_trace_msg(erase_channel, 3,
      "unable to close directory '%s' for dsync: %s", dir_path,
      strerror(xerrno)); 

    errno = xerrno;
    return -1;
  }

#endif /* !HAVE_DIRFD */

  if (file_fd > -1) {
    if (erase_fsync(p, file_fd) < 0) {
      pr_trace_msg(erase_channel, 8,
        "fsync error on fd %d: %s", file_fd, strerror(errno));
    }
  }

  return 0;
}

static int ignore_fsync_errno(int xerrno) {
  /* Some fdatasync/fsync errors can be ignored; these rules were found 
   * in the shred(1) source code. 
   */
  if (xerrno == EINVAL ||
      xerrno == EBADF ||
      xerrno == EISDIR) {
    return TRUE;
  }

  return FALSE;
}

/* Try various means of forcing the data to disk, with fallbacks. */
int erase_fsync(pool *p, int file_fd) {
  int res = -1;
  int xerrno = ENOSYS;

  if (res < 0 &&
      !ignore_fsync_errno(xerrno)) {
#if defined(F_FULLFSYNC)
    res = fcntl(file_fd, F_FULLFSYNC, NULL);
    xerrno = errno;
#endif
  }

  if (res < 0 &&
      !ignore_fsync_errno(xerrno)) {
#if defined(HAVE_FDATASYNC)
    res = fdatasync(file_fd);
    xerrno = errno;
#endif
  }

  if (res < 0 &&
      !ignore_fsync_errno(xerrno)) {
#if defined(HAVE_FSYNC)
    res = fsync(file_fd);
    xerrno = errno;
#endif
  }

  errno = xerrno;
  return res;
}
