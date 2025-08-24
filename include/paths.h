/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Save paths & helpers (portable).
 *
 * Responsibilities:
 *   - Define canonical on-disk locations for all save files.
 *   - Provide a tiny helper to format per-slot Solitaire save paths.
 *   - Keep names portable (forward slashes OK on Windows/POSIX).
 */

#ifndef PATHS_H
#define PATHS_H

/* ------------------------------------------------------------------------- */
/* Save tree layout                                                          */
/* ------------------------------------------------------------------------- */
/*
 * saves/
 *   player_data.dat
 *   achievements.dat
 *   solitaire/
 *     solitaire_save_slot_1.dat
 *     ...
 */

#define SAVE_DIR                 "saves"

/* Flat files */
#define PLAYER_DATA_PATH         SAVE_DIR "/player_data.dat"
#define ACHIEVEMENTS_PATH        SAVE_DIR "/achievements.dat"

/* Backward-compat aliases (older modules may use these names) */
#define PLAYER_SAVE_FILE         PLAYER_DATA_PATH
#define ACHIEVEMENT_SAVE         ACHIEVEMENTS_PATH

/* Solitaire saves */
#define SOLITAIRE_SAVE_DIR       SAVE_DIR "/solitaire"
#define SOLITAIRE_SLOT_BASENAME  "solitaire_save_slot_%d.dat"
#define SOLITAIRE_SLOT_FMT       SOLITAIRE_SAVE_DIR "/" SOLITAIRE_SLOT_BASENAME

/* A generous buffer size for building file paths */
#ifndef SAVE_PATH_MAX
#define SAVE_PATH_MAX 512
#endif

/* ------------------------------------------------------------------------- */
/* Helper                                                                    */
/* ------------------------------------------------------------------------- */
/**
 * solitaire_slot_path
 * Build a concrete save-file path for a given Solitaire slot.
 *
 * @param buf      Destination buffer.
 * @param bufsize  Capacity of destination buffer in bytes.
 * @param slot     1-based slot index.
 *
 * Usage:
 *   char path[SAVE_PATH_MAX];
 *   solitaire_slot_path(path, sizeof(path), 1);  // -> "saves/solitaire/solitaire_save_slot_1.dat"
 */
#include <stdio.h>
static inline void solitaire_slot_path(char *buf, size_t bufsize, int slot)
{
    (void)snprintf(buf, bufsize, SOLITAIRE_SLOT_FMT, slot);
}

#endif /* PATHS_H */
