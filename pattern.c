/*
 * ProFTPD - mod_erase patterns
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
#include "pattern.h"

#ifdef PR_USE_OPENSSL
# include <openssl/rand.h>
#endif /* PR_USE_OPENSSL */

static int rand_fill(pool *p, unsigned char *buf, size_t bufsz) {
  int res;

#if defined(PR_USE_OPENSSL)
  RAND_pseudo_bytes(buf, bufsz);
  res = 0;
#elif defined(HAVE_RANDOM)
  register unsigned int i = 0;
  unsigned int incr = SIZEOF_LONG;
  long r;

  /* random(3) returns a long, which gives us at least 32 (and maybe 64) bits
   * of random data, enough for 4 (or 8) bytes in our buffer. 
   */

  for (i = 0; i < bufsz; i += incr) {
    r = random();
 
    memcpy(&(buf[i]), &r, incr); 
  }

  /* If we haven't filled up the buffer yet, get one more number from
   * random(3), and use it for the remaining bytes.
   */
  if (i <= bufsz) {
    r = random();
    /* XXX: i is an INDEX, bufsz is a LENGTH; check for off-by-one bugs here! */
    memcpy(&(buf[i]), &r, bufsz-i);
  }

  res = 0;

#else
  errno = ENOSYS;
  res = -1;
#endif

  return res;
}

int erase_pattern_fill(pool *p, int pattern, unsigned char *buf, size_t bufsz) {
  int res = -1;

  if (p == NULL ||
      buf == NULL) {
    errno = EINVAL;
    return -1;
  }

  switch (pattern) {
    case ERASE_PATTERN_ALL_ONES:
      memset(buf, 1, bufsz);
      res = 0;
      break;

    case ERASE_PATTERN_ALL_ZEROS:
      memset(buf, 0, bufsz);
      res = 0;
      break;

    case ERASE_PATTERN_RANDOM:
      res = rand_fill(p, buf, bufsz);
      res = 0;
      break;

    default:
      errno = EPERM;
  }

  return res;
}

const char *erase_strpattern(pool *p, int pattern) {
  const char *pattern_str;

  switch (pattern) {
    case ERASE_PATTERN_ALL_ONES:
      pattern_str = "AllOnes";
      break;

    case ERASE_PATTERN_ALL_ZEROS:
      pattern_str = "AllZeros";
      break;

    case ERASE_PATTERN_RANDOM:
      pattern_str = "Random";
      break;

    default:
      pattern_str = "(unknown)";
  }

  return pattern_str;
}
