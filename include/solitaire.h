/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * Klondike Solitaire public surface + shared types
 *
 * This header exposes the core game structures and the small public API
 * used by the rest of the program (e.g., starting/ending games, save UI,
 * and the optional DFS winnability probe).
 */

#ifndef SOLITAIRE_H
#define SOLITAIRE_H

#include "core.h"

/* ------------------------------------------------------------------------- */
/* Game constants                                                            */
/* ------------------------------------------------------------------------- */

/* The maximum number of save slots. */
#define MAX_SLOTS                 5

/* Table geometry: seven columns in Klondike. */
#define COLUMNS                   7

/* Four foundations (one per suit). */
#define FOUNDATION_PILES          4

/* Capacity used for stock/waste/foundation arrays. */
#define MAX_DRAW_STACK            52

/* Max size of a completed foundation (Ace..King). */
#define MAX_FOUNDATION            13

/* Standardized difficulty identifiers (used across the codebase). */
#define DIFFICULTY_EASY           1   /* Draw 1, recycling, undo allowed. */
#define DIFFICULTY_NORMAL         2   /* Draw 1, no recycling, no undo.   */
#define DIFFICULTY_HARD           3   /* Draw 3, no recycling, no undo.   */

/* ------------------------------------------------------------------------- */
/* Core containers and game-state model                                      */
/* ------------------------------------------------------------------------- */

/**
 * Stack
 * Simple LIFO container for cards used by draw pile, waste pile, and each
 * foundation pile. The "top" card is at index (count - 1).
 *
 * Invariants:
 *   - 0 <= count <= MAX_DRAW_STACK
 *   - Elements are valid only for indices [0, count).
 */
typedef struct {
    Card cards[MAX_DRAW_STACK];
    int  count;
} Stack;

/**
 * KlondikeGame
 * Complete snapshot of a Klondike position.
 *
 * Members:
 *   - table:        7 columns of face-down/up cards; top is table[i][table_counts[i]-1].
 *   - table_counts: live size for each table column.
 *   - drawPile:     stock (cards yet to be drawn).
 *   - wastePile:    upturned cards from the stock.
 *   - foundation:   four ascending-by-suit piles (Ace->King).
 *   - difficulty:   DIFFICULTY_EASY / DIFFICULTY_NORMAL / DIFFICULTY_HARD.
 *   - undo:         set true when the player used undo this game (affects stats).
 *
 * Important notes:
 *   - Card.revealed is used on table cards to render [???] vs face-up.
 *   - For draw/waste, code treats cards as face-up when drawn; the revealed flag
 *     may be set accordingly at deal time.
 */
typedef struct {
    Card  table[COLUMNS][MAX_DRAW_STACK];
    int   table_counts[COLUMNS];
    Stack drawPile;
    Stack wastePile;
    Stack foundation[FOUNDATION_PILES];
    int   difficulty;
    bool  undo;
} KlondikeGame;

/* ------------------------------------------------------------------------- */
/* DFS / solver tunables + transposition entry                               */
/* ------------------------------------------------------------------------- */

/* Depth cap for the solver’s explicit stack (heap-backed). */
#define DFS_MAX_DEPTH             512

/* Open-addressing hash table capacity for visited states (prime for spread). */
#define VISITED_CAP               200003

/* Hard node-visit ceiling to prevent runaway searches. */
#define DFS_NODE_LIMIT            2000000

/**
 * VisitedEntry
 * Single slot in the solver’s open-addressing transposition table.
 * 'used' acts like a tombstone-free occupancy flag (no deletions needed).
 */
typedef struct {
    uint64_t key;
    int      used;
} VisitedEntry;

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * save_prompt
 * Interactively offer to save the current game into a slot and return to the
 * main menu on success. Non-blocking for the rest of the program (UI-owned).
 */
void save_prompt(KlondikeGame *game);

/**
 * dfs_solitaire_win
 * Run the DFS/heuristic solver against the given position. Used during
 * deal selection to prefer winnable boards when enabled in config.
 *
 * @return true if a forced win sequence exists from this state.
 */
bool dfs_solitaire_win(KlondikeGame *game);

#endif /* SOLITAIRE_H */
