/*
 * ProFTPD - mod_erase
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
 *
 * DO NOT EDIT BELOW THIS LINE
 * $Archive: mod_erase.a $
 */

#include "mod_erase.h"
#include "open.h"
#include "pattern.h"
#include "sync.h"
#include "erase.h"

/* Defaults */

module erase_module;

int erase_logfd = -1;
pool *erase_pool = NULL;
const char *erase_channel = "erase";

static int erase_engine = FALSE;

static int erase_openlog(void) {
  int res = 0;
  config_rec *c;

  c = find_config(main_server->conf, CONF_PARAM, "EraseLog", FALSE);
  if (c) {
    char *path;

    path = c->argv[0];

    if (strncasecmp(path, "none", 5) != 0) {
      int xerrno;

      pr_signals_block();
      PRIVS_ROOT
      res = pr_log_openfile(path, &erase_logfd, 0600);
      xerrno = errno;
      PRIVS_RELINQUISH
      pr_signals_unblock();

      if (res < 0) {
        if (res == -1) {
          pr_log_pri(PR_LOG_NOTICE, MOD_ERASE_VERSION
            ": notice: unable to open EraseLog '%s': %s", path,
            strerror(xerrno));

        } else if (res == PR_LOG_WRITABLE_DIR) {
          pr_log_pri(PR_LOG_NOTICE, MOD_ERASE_VERSION
            ": notice: unable to open EraseLog '%s': parent directory is "
            "world-writable", path);

        } else if (res == PR_LOG_SYMLINK) {
          pr_log_pri(PR_LOG_NOTICE, MOD_ERASE_VERSION
            ": notice: unable to open EraseLog '%s': cannot log to a symlink",
            path);
        }
      }
    }
  }

  return res;
}

/* FSIO Callbacks
 */

static int erase_unlink(pr_fs_t *fs, const char *path) {
  register unsigned int i;
  config_rec *c;
  int fd, res;
  struct stat st;
  unsigned char *eraser;
  size_t erasersz;
  array_header *patterns;
  char *dir_path = NULL, *ptr = NULL;
  pool *tmp_pool = NULL;

  c = find_config(CURRENT_CONF, CONF_PARAM, "EraseEnable", FALSE);
  if (c != NULL) {
    int enabled = TRUE;

    enabled = *((int *) c->argv[0]);

    if (enabled == TRUE) {
      /* Just use unlink(2) and be done. */
      return unlink(path);
    }
  }

  c = find_config(CURRENT_CONF, CONF_PARAM, "ErasePatterns", FALSE);
  if (c == NULL) {
    /* No patterns?  Juse use unlink(2) and be done. */
    return unlink(path);
  }

  patterns = c->argv[0];

  fd = erase_open(erase_pool, path, &st);
  if (fd < 0) {
    return unlink(path);
  }

  res = unlink(path);
  if (res < 0) {
    int xerrno = errno;

    (void) close(fd);
    (void) pr_log_writefile(erase_logfd, MOD_ERASE_VERSION,
      ": error unlinking '%s': %s", path, strerror(xerrno));

    errno = xerrno;
    return -1;
  }

  tmp_pool = make_sub_pool(erase_pool);
  pr_pool_tag(tmp_pool, "erase file");

  /* Sync the directory data, now that we've unlinked the file, so that
   * other programs see that the file is gone.
   */
  ptr = strrchr(path, '/');
  if (ptr != NULL) {
    dir_path = pstrndup(tmp_pool, path, (ptr - path));

  } else {
    dir_path = ".";
  }

  res = erase_dsync(tmp_pool, dir_path, fd);
  if (res < 0) {
    (void) pr_log_writefile(erase_logfd, MOD_ERASE_VERSION,
      "error updating directory '%s' data: %s", dir_path, strerror(errno));
  }

  erasersz = st.st_blksize;
  eraser = palloc(tmp_pool, erasersz);

  for (i = 0; i < patterns->nelts; i++) {
    int pattern;

    pattern = ((int *) patterns->elts)[i]; 

    /* Make sure to rewind to the start of the file. */
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
      (void) pr_log_writefile(erase_logfd, MOD_ERASE_VERSION,
        "error seeking to start of '%s': %s", path, strerror(errno));
      break;
    }

    res = erase_erase(tmp_pool, fd, st.st_size, pattern, eraser, erasersz);
    if (res < 0) {
      (void) pr_log_writefile(erase_logfd, MOD_ERASE_VERSION,
        "error erasing '%s' using '%s' pattern: %s", path,
        erase_strpattern(tmp_pool, pattern), strerror(errno));
      break;
    }
  }

  if (tmp_pool != NULL) {
    destroy_pool(tmp_pool);
  }

  if (close(fd) < 0) {
    (void) pr_log_writefile(erase_logfd, MOD_ERASE_VERSION,
      "error writing '%s': %s", path, strerror(errno));
  }

  return 0;
}

/* Command handlers
 */

MODRET erase_post_pass(cmd_rec *cmd) {
  pr_fs_t *fs;

  if (erase_engine != TRUE) {
    return PR_DECLINED(cmd);
  }

  fs = pr_register_fs(main_server->pool, "erase", "/");
  if (fs != NULL) {
    pr_log_debug(DEBUG5, MOD_ERASE_VERSION ": FS callbacks registered");
    fs->unlink = erase_unlink;

  } else {
    pr_log_debug(DEBUG0, MOD_ERASE_VERSION
      ": error registering FS callbacks: %s", strerror(errno));
  }

  return PR_DECLINED(cmd);
}

/* Configuration handlers
 */

/* usage: EraseEnable on|off */
MODRET set_eraseenable(cmd_rec *cmd) {
  int enabled = -1;
  config_rec *c;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ANON|CONF_DIR|CONF_DYNDIR);

  enabled = get_boolean(cmd, 1);
  if (enabled == -1) {
    CONF_ERROR(cmd, "expected Boolean parameter");
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = palloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = enabled;
  c->flags |= CF_MERGEDOWN;

  return PR_HANDLED(cmd);
}

/* usage: EraseEngine on|off */
MODRET set_eraseengine(cmd_rec *cmd) {
  int engine = 1;
  config_rec *c;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  engine = get_boolean(cmd, 1);
  if (engine == -1) {
    CONF_ERROR(cmd, "expected Boolean parameter");
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = engine;

  return PR_HANDLED(cmd);
}

/* usage: EraseLog path|"none" */
MODRET set_eraselog(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT);

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: ErasePatterns pattern1 pattern2 ... patternN */
MODRET set_erasepatterns(cmd_rec *cmd) {
  config_rec *c = NULL;
  register unsigned int i;
  array_header *patterns;

  if (cmd->argc-1 == 0) {
    CONF_ERROR(cmd, "wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  c = add_config_param(cmd->argv[0], 1, NULL);
  patterns = make_array(c->pool, 1, sizeof(int));

  for (i = 1; i < cmd->argc; i++) {
    if (strcmp(cmd->argv[i], "AllOnes") == 0) {
      *((int *) push_array(patterns)) = ERASE_PATTERN_ALL_ONES;

    } else if (strcmp(cmd->argv[i], "AllZeros") == 0) {
      *((int *) push_array(patterns)) = ERASE_PATTERN_ALL_ZEROS;

    } else if (strcmp(cmd->argv[i], "Random") == 0) {
      *((int *) push_array(patterns)) = ERASE_PATTERN_RANDOM;

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, ": unknown ErasePattern '",
        cmd->argv[i], "'", NULL));
    }
  }

  c->argv[0] = (void *) patterns;
  return PR_HANDLED(cmd);
}

/* Event listeners
 */

#if defined(PR_SHARED_MODULE)
static void erase_mod_unload_ev(const void *event_data, void *user_data) {
  if (strncmp((const char *) event_data, "mod_erase.c", 12) == 0) {
    register unsigned int i;

    /* Unregister ourselves from all events. */
    pr_event_unregister(&erase_module, NULL, NULL);

    destroy_pool(erase_pool);
    erase_pool = NULL;
  }
}
#endif

static void erase_restart_ev(const void *event_data, void *user_data) {
#ifdef HAVE_RANDOM
  /* Seed the random(3) generator. */ 
  srandom((unsigned int) (time(NULL) * getpid())); 
#endif /* HAVE_RANDOM */
}

/* Initialization routines
 */

static int erase_init(void) {
  erase_pool = make_sub_pool(permanent_pool);
  pr_pool_tag(erase_pool, MOD_ERASE_VERSION);

#if defined(PR_SHARED_MODULE)
  pr_event_register(&erase_module, "core.module-unload", erase_mod_unload_ev,
    NULL);
#endif
  pr_event_register(&erase_module, "core.restart", erase_restart_ev, NULL);

#ifdef HAVE_RANDOM
  /* Seed the random(3) generator. */ 
  srandom((unsigned int) (time(NULL) * getpid())); 
#endif /* HAVE_RANDOM */

  return 0;
}

static int erase_sess_init(void) {
  config_rec *c;

  c = find_config(main_server->conf, CONF_PARAM, "EraseEngine", FALSE);
  if (c) {
    erase_engine = *((int *) c->argv[0]);
  }

  if (erase_engine == FALSE) {
    return 0;
  }

  erase_openlog();

#ifdef HAVE_RANDOM
  /* Reseed the random(3) generator. */ 
  srandom((unsigned int) (time(NULL) ^ getpid())); 
#endif /* HAVE_RANDOM */

  return 0;
}

/* Module API tables
 */

static conftable erase_conftab[] = {
  { "EraseEnable",	set_eraseenable,	NULL },
  { "EraseEngine",	set_eraseengine,	NULL },
  { "EraseLog",		set_eraselog,		NULL },
  { "ErasePatterns",	set_erasepatterns,	NULL },
  { NULL }
};

static cmdtable erase_cmdtab[] = {
  { POST_CMD,	C_PASS,	G_NONE,	erase_post_pass,	FALSE,	FALSE },
  { 0, NULL }
};

module erase_module = {
  /* Always NULL */
  NULL, NULL,

  /* Module API version */
  0x20,

  /* Module name */
  "erase",

  /* Module configuration handler table */
  erase_conftab,

  /* Module command handler table */
  erase_cmdtab,

  /* Module authentication handler table */
  NULL,

  /* Module initialization */
  erase_init,

  /* Session initialization */
  erase_sess_init,

  /* Module version */
  MOD_ERASE_VERSION
};

