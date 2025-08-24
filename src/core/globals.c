/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Global state definitions + normalization.
 *
 * Responsibilities:
 *   - Define global PlayerData and GameConfig (extern in core.h).
 *   - Provide globals_init() to clamp/normalize config ranges on boot.
 */

#include "core.h"

/* ------------------------------------------------------------------------- */
/* Single definitions of globals                                             */
/* ------------------------------------------------------------------------- */

PlayerData playerData = (PlayerData){0};

GameConfig config = {
    .jokers             = false,
    .num_decks          = 1,      /* Blackjack reads this (1..8).           */
    .autosave           = 0,      /* minutes; 0 disables                     */
    .depth_first_search = false,  /* Solitaire deal selection (optional)     */
    .backtracking       = false   /* placeholder flag                        */
};

/* ------------------------------------------------------------------------- */
/* Optional normalization bootstrap                                          */
/* ------------------------------------------------------------------------- */

/**
 * globals_init
 * Clamp config values to safe ranges and coerce booleans.
 * Cheap guard against garbage values after loading persisted data.
 */
void globals_init(void)
{
    if (config.num_decks < 1)       config.num_decks = 1;
    else if (config.num_decks > 8)  config.num_decks = 8;

    if (config.autosave < 0)        config.autosave  = 0;
    else if (config.autosave > 60)  config.autosave  = 60;

    config.jokers             = !!config.jokers;
    config.depth_first_search = !!config.depth_first_search;
    config.backtracking       = !!config.backtracking;
}
