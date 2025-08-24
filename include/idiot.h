/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Idiot card game — public header.
 *
 * This header exposes the minimal public API and data structures used by the
 * Idiot game mode. All game logic is implemented in idiot.c; helpers that are
 * not intended for use outside the translation unit remain file-local there.
 *
 * Summary of rules:
 *   - Each player receives 3 face-down cards, 3 face-up cards, and 3 hand cards.
 *   - You must play a card equal to or higher than the top of the waste pile.
 *   - Special cards:
 *       2  resets the pile restriction (anything may follow).
 *       3  mirrors through 3’s (and Jokers) to the last non-3 lock.
 *       10 burns the pile (removes all cards on it).
 *   - Four of a kind on top also burns the pile.
 *   - Jokers behave exactly like a 3 (wild mirror) and may be played anytime.
 *   - If you cannot play, you must pick up the entire waste pile.
 */

#ifndef IDIOT_H
#define IDIOT_H

#include "core.h"  /* Card, DECK_SIZE, initialize_deck, shuffle_deck, etc. */

/* ------------------------------------------------------------------------- */
/* Game constants                                                            */
/* ------------------------------------------------------------------------- */

#define HAND_SIZE            3      /* Number of cards in hand */
#define FACE_UP_SIZE         3      /* Number of face-up cards */
#define FACE_DOWN_SIZE       3      /* Number of face-down cards */
#define MAX_PILE             60     /* Enough space for whole deck + Jokers */
#define MAX_HAND_CARDS       60     /* Allow picking up the entire pile */
#define LASTMOVE_MAX         2      /* UI shows up to 2 cards per AI turn */

/* Standardized difficulty identifiers (used across the codebase). */
#define DIFFICULTY_EASY           1   /* Easy difficulty */
#define DIFFICULTY_NORMAL         2   /* Normal difficulty */
#define DIFFICULTY_HARD           3   /* Hard difficulty */

/**
 * IdiotPlayer
 * Aggregates all player zones and their dynamic counts.
 */
typedef struct {
    Card hand        [MAX_HAND_CARDS];
    Card faceUp      [FACE_UP_SIZE];
    Card faceDown    [FACE_DOWN_SIZE];
    int  handCount;
    int  faceUpCount;
    int  faceDownCount;
} IdiotPlayer;

/**
 * CardPile
 * Generic pile stack used for both draw and waste piles.
 */
typedef struct {
    Card pile [MAX_PILE];
    int  count;
} CardPile;

/**
 * AILastMove
 * Small structure used only for UI: summarizes what the AI just did so the
 * player can see it rendered (cards played, whether a burn happened, and any
 * “mirrored” target when playing a 3/Joker).
 */
typedef struct {
    int   playedCount;                        /* 0..LASTMOVE_MAX */
    Card  played[LASTMOVE_MAX];               /* Cards AI played this visible turn */
    int   burned;                             /* 1 if 10/four-of-a-kind burn occurred */
    int   mirrored;                           /* 1 if last play mirrored a lock */
    Card *mirroredCard;                       /* The mirrored target (may be NULL) */
} AILastMove;

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

/**
 * idiotHowToPlay
 * Clear the screen and print a rule/tutorial page for Idiot.
 * Blocks until the user presses Enter.
 */
void idiotHowToPlay(void);

/**
 * idiot_start
 * Entry point for the Idiot game mode.
 *
 * Responsibilities:
 *  - Ask for difficulty and wager (wagers for Normal/Hard only).
 *  - Deal, optionally add Jokers, and let the player swap face-up cards.
 *  - Run the interactive loop (player vs AI) until someone wins.
 *  - Update player money, achievements, and persistent stats.
 */
void idiot_start(void);

#endif /* IDIOT_H */
