/*
 * ProFTPD - mod_erase iterations
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

#ifndef MOD_ERASE_PATTERN_H
#define MOD_ERASE_PATTERN_H

/* mod_erase patterns */
#define ERASE_PATTERN_ALL_ONES		1
#define ERASE_PATTERN_ALL_ZEROS		2
#define ERASE_PATTERN_RANDOM		3

/* Given a buffer, fill it with the data appropriate for the pattern. */
int erase_pattern_fill(pool *p, int pattern, unsigned char *buf, size_t bufsz);

const char *erase_strpattern(pool *p, int pattern);

#endif
