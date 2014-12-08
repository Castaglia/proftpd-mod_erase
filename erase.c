/*
 * ProFTPD - mod_erase erase
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
#include "erase.h"
#include "pattern.h"
#include "sync.h"

/* After every write, we need to refill our eraser with its pattern data.
 *
 * For the AllOnes and AllZeros patterns, this is a no-op.  But for the Random
 * pattern, it is necessary.
 */
static int refill_pattern(pool *p, int pattern, unsigned char *eraser,
    size_t erasersz) {
  int res = 0;

  switch (pattern) {
    case ERASE_PATTERN_ALL_ONES:
    case ERASE_PATTERN_ALL_ZEROS:
      break;

    case ERASE_PATTERN_RANDOM:
      res = erase_pattern_fill(p, pattern, eraser, erasersz);
      if (res < 0) {
        int xerrno = errno;

        pr_trace_msg(erase_channel, 6,
          "error filling buffer for 'Random' pattern: %s", strerror(errno));

        errno = xerrno;
        res = -1;
      }
      break; 
  }

  return res;
}

static int write_eraser(int fd, unsigned char *eraser, size_t erasersz) {
  const unsigned char *ptr;
  size_t rem;

  ptr = eraser;
  rem = erasersz;

  while (rem > 0) {
    ssize_t nwritten;

    pr_signals_handle();

    nwritten = write(fd, ptr, rem);
    if (nwritten < 0) {
      int xerrno = errno;

      if (xerrno == EINTR) {
        pr_signals_handle();
        continue;
      }
    }

    rem -= nwritten;
    ptr += nwritten;
  }

  return 0;
}

int erase_erase(pool *p, int fd, off_t filesz, int pattern,
    unsigned char *eraser, size_t erasersz) {
  int res;
  off_t max, pos = 0;

  if (p == NULL ||
      eraser == NULL) {
    errno = EINVAL;
    return -1;
  }

  res = erase_pattern_fill(p, pattern, eraser, erasersz);
  if (res < 0) {
    int xerrno = errno;

    pr_trace_msg(erase_channel, 6,
      "error filling buffer for '%s' pattern: %s", erase_strpattern(p, pattern),
      strerror(errno));

    errno = xerrno;
    return -1;
  }

  /* Always remember that pos is an INDEX, and filesz is a LENGTH.  Thus the
   * max position value is filesz-1.
   */
  max = filesz - 1;

  while (pos < max) {
    size_t eraser_len;
    off_t eraser_rem;

    pr_signals_handle();

    eraser_len = erasersz;
    eraser_rem = max - pos;
    if (eraser_rem < eraser_len) {
      eraser_len = (size_t) eraser_rem;
    }

    res = write_eraser(fd, eraser, eraser_len);
    if (res < 0) {
      break;
    }

    /* Note: Should this sync happen after every write, or only after
     * the entire filesz has been written?  Tunable, perhaps, for performance?
     */
    if (erase_fsync(p, fd) < 0) {
      pr_trace_msg(erase_channel, 7,
        "error flushing data to disk for fd %d: %s", fd, strerror(errno));
    }

    pos += eraser_len;

    if (pos < max) {
      res = refill_pattern(p, pattern, eraser, erasersz);
      if (res < 0) {
        break;
      }
    }
  }

  return res;
}
