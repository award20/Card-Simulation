/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * Core types + globals shared across the card games
 *
 * Exposes:
 *   - Card, GameConfig, GameStats, PlayerData.
 *   - Common helpers (deck init/shuffle/print, clear_screen).
 *   - Top-level menus callable from other modules.
 */

#ifndef DECK_H
#define DECK_H

/* ------------------------------------------------------------------------- */
/* Standard headers                                                          */
/* ------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include "achievements.h"   /* Needed for achievement types, API. */

#ifdef _WIN32
  #include <windows.h>
#else
  /* Sleep shim if ever compiled on non-Windows; not used directly in header. */
  #include <unistd.h>
#endif

/* ------------------------------------------------------------------------- */
/* Card constants                                                            */
/* ------------------------------------------------------------------------- */

#define NUM_SUITS   4
#define NUM_RANKS   13
#define DECK_SIZE   52

/* ------------------------------------------------------------------------- */
/* Data types                                                                */
/* ------------------------------------------------------------------------- */

/**
 * Card
 * A playing card identified by (suit, rank).
 * 'revealed' is used by Solitaire to control face-down rendering.
 * 'is_joker' is used by games that allow jokers (e.g., Idiot).
 */
typedef struct {
    char *suit;
    char *rank;
    int   revealed;
    bool  is_joker;
} Card;

/**
 * GameConfig
 * Global rules that affect game modes.
 *
 * - jokers:            include jokers where supported.
 * - num_decks:         1..8 (Blackjack uses a shoe).
 * - autosave:          minutes between autosaves (0 = off).
 * - depth_first_search:enable DFS deal selection for Solitaire.
 * - backtracking:      placeholder flag for a future solver mode.
 */
typedef struct {
    bool jokers;
    int  num_decks;
    int  autosave;
    bool depth_first_search;
    bool backtracking;          /* currently not implemented */
} GameConfig;

/**
 * GameStats
 * Per-game statistics (wins/losses/draws) + achievement counters.
 */
typedef struct {
    int wins;
    int losses;
    int draws;
    int win_streak;
    int max_win_streak;

    /* Blackjack-specific achievement counters */
    int blackjack_wins;         /* natural 21 on opening hand */
    int doubledown_wins;
    int insurance_success;
    int split_wins;

    /* Solitaire-specific */
    int perfect_clear;
    int easy_wins;
    int normal_wins;
    int hard_wins;
    int longest_game_minutes;

    /* Idiot-specific */
    int mirror_match;
    int burns;
    int four_of_a_kind_burns;
    int trickster_wins;
} GameStats;

/**
 * PlayerData
 * Whole-profile stats and balances persisted across sessions.
 */
typedef struct {
    GameStats         blackjack;
    GameStats         solitaire;
    GameStats         idiot;

    unsigned long long uPlayerMoney;
    unsigned long long starting_balance;

    int time_played_hours;
    int time_played_minutes;
    int time_played_seconds;

    /* Aggregate counters for achievements and rolls-ups */
    int games_played;
    int total_wins;
    int total_losses;
    int total_draws;
} PlayerData;

/* ------------------------------------------------------------------------- */
/* Globals                                                                   */
/* ------------------------------------------------------------------------- */

extern PlayerData playerData;
extern GameConfig config;

/* ------------------------------------------------------------------------- */
/* Shared helpers (defined in main.c)                                        */
/* ------------------------------------------------------------------------- */

bool save_player_data(void);
bool load_player_data(void);

void globals_init(void);
void fs_init(void);

void initialize_deck(Card *deck);
void shuffle_deck(Card *deck);
void print_deck(Card *deck);
void clear_screen(void);
void pause_for_enter(void);

/* Top-level navigation (defined in main.c) */
void deckMenu(void);
void gamesMenu(void);
void changeFunds(void);
void otherMenu(void);
void resetStatistics(void);
void resetAchievements(void);
void gameRules(void);
void customRules(void);
void jokers(void);
void numberOfDecks(void);
void ensureWinnableSolutions(void);
void autosave_menu(void);
void statsDisplay(void);
void printAchievements(void);

/* ------------------------------------------------------------------------- */
/* Game entry points (menus/launchers provided elsewhere)                    */
/* ------------------------------------------------------------------------- */

void blackjack(void);   /* Blackjack UI/menu (not the inner loop) */
void solitaire(void);   /* Solitaire UI/menu                       */
void idiot(void);       /* Idiot UI/menu                           */

/* Blackjack gameplay helpers (from blackjack.c) */
void blackjackHowToPlay(void);
void blackjack_start(void);

/* Solitaire helpers */
void solitaireHowToPlay(void);
void solitaire_start(void);

/* Idiot helpers */
void idiotHowToPlay(void);
void idiot_start(void);

#endif /* DECK_H */
