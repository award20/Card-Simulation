// src/core/paths.h
#ifndef PATHS_H
#define PATHS_H

// simple, portable save layout:
//
// saves/
//   player_data.dat
//   achievements.dat
//   solitaire/
//     solitaire_save_slot_1.dat
//     ...
//
// forward slashes work on Windows CRT and POSIX.

#define SAVE_DIR                 "saves"

// flat files
#define PLAYER_DATA_PATH         SAVE_DIR "/player_data.dat"
#define ACHIEVEMENTS_PATH        SAVE_DIR "/achievements.dat"

// solitaire saves
#define SOLITAIRE_SAVE_DIR       SAVE_DIR "/solitaire"
#define SOLITAIRE_SLOT_BASENAME  "solitaire_save_slot_%d.dat"
#define SOLITAIRE_SLOT_FMT       SOLITAIRE_SAVE_DIR "/" SOLITAIRE_SLOT_BASENAME

// a generous buffer size for building file paths
#ifndef SAVE_PATH_MAX
#define SAVE_PATH_MAX 512
#endif

// tiny helper to format a slot path safely
// usage:
//   char path[SAVE_PATH_MAX];
//   solitaire_slot_path(path, sizeof(path), slot);
#include <stdio.h>
static inline void solitaire_slot_path(char *buf, size_t bufsize, int slot) {
    // snprintf returns characters that *would* have been written; we don't need it here.
    (void)snprintf(buf, bufsize, SOLITAIRE_SLOT_FMT, slot);
}

#endif /* PATHS_H */
