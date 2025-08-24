/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Filesystem bootstrap helpers.
 *
 * Responsibilities:
 *   - Ensure save directories exist at startup.
 *   - Touch base save files so later loads don't fail.
 *   - Cross-platform mkdir abstraction.
 */

#include "core.h"
#include "paths.h"
#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
  #include <direct.h>   /* _mkdir */
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define MKDIR(p) mkdir((p), 0755)
#endif

/* ------------------------------------------------------------------------- */
/* Path fallbacks (only used if paths.h leaves these undefined)              */
/* ------------------------------------------------------------------------- */
#ifndef SAVE_DIR
  #define SAVE_DIR "saves"
#endif
#ifndef PLAYER_SAVE_FILE
  #define PLAYER_SAVE_FILE "saves/player_data.dat"
#endif
#ifndef ACHIEVEMENT_SAVE
  #define ACHIEVEMENT_SAVE "saves/achievements.dat"
#endif
#ifndef SOLITAIRE_SAVE_DIR
  #define SOLITAIRE_SAVE_DIR "saves/solitaire"
#endif

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/**
 * ensure_dir
 * Try to create a directory. If it already exists, do nothing.
 * Errors are ignored to keep the CLI quiet.
 */
static void ensure_dir(const char *path)
{
    if (!path || !*path) return;
    if (MKDIR(path) == 0) return;      /* created OK */
    if (errno == EEXIST) return;       /* already exists */
    /* Otherwise ignore. */
}

/**
 * touch_file
 * Create the file if missing; leave contents untouched if present.
 */
static void touch_file(const char *path)
{
    if (!path || !*path) return;
    FILE *f = fopen(path, "ab+");  /* create if missing, keep contents if present */
    if (f) fclose(f);
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * fs_init
 * Create the save root + solitaire subdir and ensure the two main save files
 * exist so first-run code paths that try to load won't fail noisily.
 */
void fs_init(void)
{
    ensure_dir(SAVE_DIR);
    ensure_dir(SOLITAIRE_SAVE_DIR);
    touch_file(PLAYER_SAVE_FILE);
    touch_file(ACHIEVEMENT_SAVE);
}
