/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Klondike Solitaire game implementation.
 *
 * This file contains the game logic for Klondike Solitaire,
 * including the setup, rules, and actions for playing the game.
 * It allows users to interact with the table (7 columns), draw pile,
 * waste pile, and four foundation piles (one per suit), with the goal of
 * moving all cards to the foundations in ascending rank (Ace->King).
 *
 * The file also includes a depth-first search (DFS) solver used to probe
 * whether a freshly dealt board is winnable. The solver uses:
 *   - a transposition table (visited-state hash set),
 *   - move ordering and pruning heuristics (e.g., safe-to-foundation),
 *   - a forced move pass that collapses obvious/“safe” moves prior to branching.
 *
 * None of this affects user play; it runs only during deal selection when
 * config.depth_first_search is set to true.
 */

#include "solitaire.h"

/* Local compile-time constants. */
#define SAVE_FILE_NAME_LEN        256

/* ------------------------------------------------------------------------- */
/* Internal / file-local forward declarations                                */
/* ------------------------------------------------------------------------- */

/* Save/load helpers */
static bool save_slot_exists(int saveSlotNumber);
static int  load_game_from_slot(KlondikeGame *gameState, int saveSlotNumber);

/* Game lifecycle / UI */
static void deal_new_klondike_game(KlondikeGame *gameState, Card *shuffledDeck);  /* Deal and initialize a new game.   */
static void render_game_ascii(const KlondikeGame *gameState);                      /* ASCII render for CLI.             */
static void draw_from_stock(KlondikeGame *gameState);                              /* Stock -> waste (and recycle).     */
static bool run_game_loop(KlondikeGame *gameState, unsigned int betAmount);        /* Interactive loop.                 */

/* Basic rule checks / utilities */
static int  is_legal_foundation_placement(Card candidateCard, const Stack *foundationStack);
static int  is_legal_table_placement(Card movingCard, Card destinationCard);
static int  is_red_suit(Card card);
static void snapshot_for_undo(const KlondikeGame *gameState);
static void restore_undo(KlondikeGame *gameState);
static void perform_auto_complete(KlondikeGame *gameState);
static int  is_safe_to_auto_complete(const KlondikeGame *gameState);
static int  get_card_rank_value(Card card);

/* ------------------------------------------------------------------------- */
/* Solver pruning helpers (internal)                                         */
/* ------------------------------------------------------------------------- */

/* Return 1 if pushing card to foundations is “safe” by heuristic. */
static int  is_safe_foundation_push(Card candidateCard, const KlondikeGame *gameState);

/* Apply all forced (safe) foundation moves repeatedly; return 1 if changed. */
static int  apply_forced_moves(KlondikeGame *gameState);

/* Coarse predicates to prune dead ends quickly. */
static int  exists_any_table_to_table_move(const KlondikeGame *gameState);  /* Any legal table->table move? */
static int  exists_any_waste_to_table_move(const KlondikeGame *gameState);  /* Waste top fits anywhere?     */
static int  exists_any_safe_foundation_push(const KlondikeGame *gameState);
static int  exists_any_progress_move(const KlondikeGame *gameState);

/* Table sequence helpers. */
static int  can_move_sequence_onto_column(const KlondikeGame *gameState, int fromColumnIndex, int startRowIndex, int toColumnIndex);
static void apply_move_sequence_between_columns(KlondikeGame *gameState, int fromColumnIndex, int startRowIndex, int toColumnIndex);

/* Hashing utilities for solver. */
static uint64_t rotl64_u(uint64_t value64, int rotateBits);
static int      encode_rank_to_id(const char *rankStr);
static int      encode_suit_to_id(const char *suitStr);
static uint64_t compute_card_hash(Card card);
static uint64_t compute_state_hash(const KlondikeGame *gameState);

/* Transposition helpers. */
static int  visited_table_contains(VisitedEntry *visitedTable, uint64_t stateKey);
static void visited_table_insert(VisitedEntry *visitedTable, uint64_t stateKey);

/* DFS core. */
static int  dfs_search_inner(KlondikeGame *statesArray, int searchDepth, VisitedEntry *visitedTable);

/* ------------------------------------------------------------------------- */
/* Save/undo buffer (file-scope)                                             */
/* ------------------------------------------------------------------------- */

/* One-level undo snapshot and flag (Easy mode only). */
static KlondikeGame g_UndoSnapshotBuffer;
static int          g_HasUndoSnapshot = 0;

/* ------------------------------------------------------------------------- */
/* Save / Load                                                                */
/* ------------------------------------------------------------------------- */

/**
 * save_game
 * Persist the entire game state and player money to a given numbered slot.
 *
 * @param gameState     Pointer to the game state to save.
 * @param saveSlotNumber  Slot number [1..MAX_SLOTS].
 * @return 1 on success, 0 on failure (e.g., file open error).
 *
 * Side-effects: Writes a binary blob for game + money into "saves/solitaire/".
 * Errors are silent except the return value (no perror to avoid noisy UI).
 */
static int save_game(KlondikeGame *gameState, int saveSlotNumber)
{
    char saveFilePath[SAVE_FILE_NAME_LEN];

    sprintf(saveFilePath, "saves/solitaire/solitaire_save_slot_%d.dat", saveSlotNumber);

    FILE *filePtr = fopen(saveFilePath, "wb");
    if (!filePtr) { return 0; }

    fwrite(gameState, sizeof(KlondikeGame), 1, filePtr);
    fwrite(&playerData.uPlayerMoney, sizeof(unsigned long long), 1, filePtr);
    fclose(filePtr);
    return 1;
}

/**
 * save_slot_exists
 * Check if a given save slot has an existing file.
 *
 * @param saveSlotNumber  Slot number [1..MAX_SLOTS].
 * @return true if file can be opened for reading, false otherwise.
 */
static bool save_slot_exists(int saveSlotNumber)
{
    char saveFilePath[SAVE_FILE_NAME_LEN];

    sprintf(saveFilePath, "saves/solitaire/solitaire_save_slot_%d.dat", saveSlotNumber);

    FILE *filePtr = fopen(saveFilePath, "rb");
    if (filePtr)
    {
        fclose(filePtr);
        return true;
    }
    return false;
}

/**
 * print_slots
 * Utility to list all slots with a quick "occupied/empty" hint for the user.
 * (Used by the save/load prompts.)
 */
static void print_slots(void)
{
    for (int slotIndex = 1; slotIndex <= MAX_SLOTS; ++slotIndex)
    {
        char saveFilePath[SAVE_FILE_NAME_LEN];
        sprintf(saveFilePath, "saves/solitaire/solitaire_save_slot_%d.dat", slotIndex);

        if (save_slot_exists(slotIndex))
        {
            printf("Slot %d: %s\n", slotIndex, saveFilePath);
        }
        else
        {
            printf("Slot %d: empty\n", slotIndex);
        }
    }
}

/**
 * save_prompt
 * Interactively ask the player whether/where to save after a game. We:
 *  - detect how many slots exist to customize the message,
 *  - handle confirmation on overwrite,
 *  - loop until a valid selection is made or the user cancels.
 *
 * NOTE: On successful save, we jump back into the main menu (solitaire()).
 * This mirrors the existing program flow.
 */
void save_prompt(KlondikeGame *gameState)
{
    int numExistingSaveSlots = 0;

    for (int slotIndex = 1; slotIndex <= MAX_SLOTS; ++slotIndex)
    {
        if (save_slot_exists(slotIndex)) { ++numExistingSaveSlots; }
    }

    if (numExistingSaveSlots == 0)
    {
        printf("Would you like to save the game?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");
    }
    else if (numExistingSaveSlots == 1)
    {
        printf("A saved game was found. Would you like to save the game?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");
    }
    else
    {
        printf("Multiple saved games were found. Would you like to save the game?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");
    }

    int userMenuChoice = 0;
    scanf("%d", &userMenuChoice);
    if (userMenuChoice != 1) { return; }

    while (1)
    {
        print_slots();
        printf("Select a slot to save to (1-%d)?\n", MAX_SLOTS);
        printf("> ");

        int userChosenSlotNumber = 0;
        scanf("%d", &userChosenSlotNumber);

        if (userChosenSlotNumber < 1 || userChosenSlotNumber > MAX_SLOTS) { continue; }

        if (save_slot_exists(userChosenSlotNumber))
        {
            printf("Do you wish to override solitaire_save_slot_%d.dat?\n", userChosenSlotNumber);
            printf("1: Yes\n");
            printf("2: No\n");
            printf("> ");

            int userOverwriteConfirmation = 0;
            scanf("%d", &userOverwriteConfirmation);

            if (userOverwriteConfirmation == 1)
            {
                save_game(gameState, userChosenSlotNumber);
                printf("Game saved to slot %d.\n", userChosenSlotNumber);
                solitaire();
                break;
            } /* else loop back to slot selection */
        }
        else
        {
            /* Slot empty: save immediately */
            save_game(gameState, userChosenSlotNumber);
            printf("Game saved to slot %d.\n", userChosenSlotNumber);
            solitaire();
            break;
        }
    }
}

/**
 * load_game_from_slot
 * Load game state and money from a specific slot.
 *
 * @param gameState       Destination game struct (out).
 * @param saveSlotNumber  Slot number [1..MAX_SLOTS].
 * @return 1 on success, 0 on failure (e.g., missing file).
 */
static int load_game_from_slot(KlondikeGame *gameState, int saveSlotNumber)
{
    char saveFilePath[SAVE_FILE_NAME_LEN];

    sprintf(saveFilePath, "saves/solitaire/solitaire_save_slot_%d.dat", saveSlotNumber);

    FILE *filePtr = fopen(saveFilePath, "rb");
    if (!filePtr) { return 0; }

    fread(gameState, sizeof(KlondikeGame), 1, filePtr);
    fread(&playerData.uPlayerMoney, sizeof(unsigned long long), 1, filePtr);
    fclose(filePtr);
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Undo                                                                       */
/* ------------------------------------------------------------------------- */

/**
 * snapshot_for_undo
 * Snapshot the current game state so the next move can be undone.
 * Only one level of undo is supported (easy mode only, by design).
 */
static void snapshot_for_undo(const KlondikeGame *gameState)
{
    memcpy(&g_UndoSnapshotBuffer, gameState, sizeof(KlondikeGame));
    g_HasUndoSnapshot = 1;
}

/**
 * restore_undo
 * Restore the last snapshot if available, otherwise notify the user.
 */
static void restore_undo(KlondikeGame *gameState)
{
    if (g_HasUndoSnapshot)
    {
        memcpy(gameState, &g_UndoSnapshotBuffer, sizeof(KlondikeGame));
        g_HasUndoSnapshot = 0;
    }
    else
    {
        printf("\nNo undo available.\n");
    }
}

/* ------------------------------------------------------------------------- */
/* How-to-Play UI                                                             */
/* ------------------------------------------------------------------------- */

/**
 * solitaireHowToPlay
 * Clear the screen and print a short rule/tutorial page for Klondike.
 * Blocks until the user presses Enter.
 */
void solitaireHowToPlay(void)
{
    clear_screen();
    printf("=== HOW TO PLAY: KLONDIKE SOLITAIRE ===\n\n");

    /* Objective */
    printf("--- Objective ---\n");
    printf("The goal of Klondike Solitaire is to move all the cards to the foundation piles.\n");
    printf("Cards must be sorted by suit in ascending order, from Ace to King.\n\n");

    /* Setup */
    printf("--- Setup ---\n");
    printf("1. Seven table columns with cards laid face down, with only the top card face up.\n");
    printf("2. A draw pile containing the remaining cards.\n");
    printf("3. Four foundation piles for each suit.\n\n");

    /* Controls */
    printf("--- Controls ---\n");
    printf("1. Move cards between table columns and foundation piles.\n");
    printf("2. Only King can be moved to an empty table column.\n");
    printf("3. Cards must follow alternating colors in descending order in table columns.\n");
    printf("4. Cards can be drawn from the draw pile to the waste pile.\n\n");

    /* Difficulty */
    printf("--- Difficulty Levels ---\n");
    printf("1. Easy: Draw 1 card at a time, with recycling draw pile.\n");
    printf("2. Normal: Draw 1 card at a time, no recycling.\n");
    printf("3. Hard: Draw 3 cards at a time, no recycling.\n\n");

    /* Win condition */
    printf("--- Winning ---\n");
    printf("You win when all cards are moved to the foundation piles in the correct order.\n");

    pause_for_enter();
    clear_screen();
}

/* ------------------------------------------------------------------------- */
/* Game loop + deal selection                                                 */
/* ------------------------------------------------------------------------- */

/**
 * solitaire_start
 * Top-level entry point for the Solitaire mode.
 *
 * Responsibilities:
 *  - Seed RNG and optionally load a saved game.
 *  - Ask for difficulty and (if applicable) a wager.
 *  - Deal a board. If config.depth_first_search is set, we iterate random deals
 *    up to a cap until the DFS solver proves one is winnable. Otherwise, deal
 *    randomly.
 *  - Run the interactive gameplay loop; upon finish, update stats/payouts.
 */
void solitaire_start(void)
{
    KlondikeGame gameState;
    int          didPlayerWin = 0;

    srand(time(NULL));  /* Randomize deals */

    /* Track total session time for achievements/stats. */
    time_t sessionStartTimestamp = time(NULL);

    /* Detect available save slots (for load prompt). */
    int numExistingSaveSlots = 0;
    int firstExistingSlot    = 0;

    for (int slotIndex = 1; slotIndex <= MAX_SLOTS; ++slotIndex)
    {
        if (save_slot_exists(slotIndex))
        {
            ++numExistingSaveSlots;
            if (firstExistingSlot == 0) { firstExistingSlot = slotIndex; }
        }
    }

    /* If exactly one save is present, offer to load it directly. */
    if (numExistingSaveSlots == 1)
    {
        if (load_game_from_slot(&gameState, firstExistingSlot))
        {
            printf("A saved game was found in slot %d. Would you like to load it?\n", firstExistingSlot);
            printf("1: Yes\n");
            printf("2: No\n");
            printf("> ");

            int userLoadChoice = 0;
            scanf("%d", &userLoadChoice);

            if (userLoadChoice == 1)
            {
                didPlayerWin = run_game_loop(&gameState, 0);
                goto after_game;
            }
            else
            {
                goto fresh_game;  /* Start a fresh game */
            }
        }
    }
    /* If multiple saves exist, list them and allow selection. */
    else if (numExistingSaveSlots > 1)
    {
        printf("Multiple saved games were found. Would you like to load one of them?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");

        int userMenuChoice = 0;
        scanf("%d", &userMenuChoice);

        if (userMenuChoice == 1)
        {
            print_slots();
            int userSelectedSlot = 0;

            printf("Slot number (1-%d): ", MAX_SLOTS);
            scanf("%d", &userSelectedSlot);

            if (userSelectedSlot >= 1 && userSelectedSlot <= MAX_SLOTS && save_slot_exists(userSelectedSlot))
            {
                if (load_game_from_slot(&gameState, userSelectedSlot))
                {
                    printf("Loaded game from slot %d.\n", userSelectedSlot);
                    didPlayerWin = run_game_loop(&gameState, 0);
                    goto after_game;
                }
            }
        }
        else
        {
            goto fresh_game;  /* Start a fresh game */
        }
    }

fresh_game:
    /* Difficulty selection. */
    printf("\n=== Select Difficulty ===\n");
    printf("1: Easy\n");
    printf("2: Normal\n");
    printf("3: Hard\n");
    printf("> ");

    scanf("%d", &gameState.difficulty);

    /* Optional betting (non-Easy only). */
    unsigned int betAmount   = 0;
    unsigned int minBet      = 10;
    unsigned int maxBet      = 100;

    if (gameState.difficulty == DIFFICULTY_NORMAL || gameState.difficulty == DIFFICULTY_HARD)
    {
        do
        {
            if (playerData.uPlayerMoney > maxBet)
            {
                printf("Enter your bet ($%u - $%u): ", minBet, maxBet);
            }
            else
            {
                printf("Enter your bet ($%u - $%lld): ", minBet, playerData.uPlayerMoney);
            }
            scanf("%u", &betAmount);
        }
        while (betAmount < minBet || betAmount > maxBet || betAmount > playerData.uPlayerMoney);

        /* Deduct bet up-front. */
        playerData.uPlayerMoney -= betAmount;
    }

    /* Deal a board. Optionally loop until the DFS solver proves it's winnable. */
    Card shuffledDeck[DECK_SIZE];
    bool isWinnableDeal = false;
    int  maxDealAttempts = 1000;

    if (config.depth_first_search)
    {
        for (int attemptIndex = 0; attemptIndex < maxDealAttempts; ++attemptIndex)
        {
            initialize_deck(shuffledDeck);

            for (int cardIndex = 0; cardIndex < DECK_SIZE; ++cardIndex)
            {
                shuffledDeck[cardIndex].revealed = 0;
            }

            shuffle_deck(shuffledDeck);
            deal_new_klondike_game(&gameState, shuffledDeck);

            if (dfs_solitaire_win(&gameState))
            {
                isWinnableDeal = true;
                break;
            }
        }

        if (!isWinnableDeal)
        {
            /* Graceful fallback to a random board if we fail to find a solvable one. */
            printf("Could not generate a winnable board after %d attempts.\n", maxDealAttempts);
            printf("Generating a random board.\n");
            pause_for_enter();

            initialize_deck(shuffledDeck);
            for (int cardIndex = 0; cardIndex < DECK_SIZE; ++cardIndex) { shuffledDeck[cardIndex].revealed = 0; }
            shuffle_deck(shuffledDeck);
            deal_new_klondike_game(&gameState, shuffledDeck);
        }
    }
    else
    {
        /* Plain random deal. */
        initialize_deck(shuffledDeck);
        for (int cardIndex = 0; cardIndex < DECK_SIZE; ++cardIndex) { shuffledDeck[cardIndex].revealed = 0; }
        shuffle_deck(shuffledDeck);
        deal_new_klondike_game(&gameState, shuffledDeck);
    }

    /* Main gameplay loop (blocking until user quits or wins). */
    didPlayerWin = run_game_loop(&gameState, betAmount);

after_game:
    /* Payouts, streaks, and stats update. */
    if (didPlayerWin)
    {
        clear_screen();

        if (gameState.difficulty == DIFFICULTY_NORMAL)
        {
            playerData.uPlayerMoney += betAmount * 2U;
            printf("You win! Earned 2x your bet: $%u\n", betAmount * 2U);
            playerData.solitaire.normal_wins++;
        }
        else if (gameState.difficulty == DIFFICULTY_HARD)
        {
            playerData.uPlayerMoney += betAmount * 5U;
            printf("You win! Earned 5x your bet: $%u\n", betAmount * 5U);
            playerData.solitaire.hard_wins++;
        }
        else
        {
            printf("You win! (Easy Mode).\n");
            playerData.solitaire.easy_wins++;
        }

        playerData.solitaire.wins++;

        if (playerData.solitaire.max_win_streak <= playerData.solitaire.win_streak)
        {
            playerData.solitaire.max_win_streak = playerData.solitaire.win_streak;
        }

        playerData.games_played++;
        playerData.total_wins++;

        if (gameState.undo) { playerData.solitaire.perfect_clear++; }

        time_t sessionEndTimestamp = time(NULL);
        int    elapsed_minutes     = (int)(difftime(sessionEndTimestamp, sessionStartTimestamp) / 60.0);
        playerData.solitaire.longest_game_minutes = elapsed_minutes;

        checkAchievements();
        save_player_data();
        save_achievements();
    }
    else
    {
        /* Loss path (also used when user quits the game loop). */
        printf("\nGame over. You did not complete all foundations.\n");

        if (gameState.difficulty == DIFFICULTY_NORMAL)
        {
            playerData.uPlayerMoney -= betAmount;
            printf("You lose your bet of $%u.\n", betAmount);
        }
        else if (gameState.difficulty == DIFFICULTY_HARD)
        {
            playerData.uPlayerMoney -= betAmount;
            printf("You lose your bet of $%u.\n", betAmount);
        }

        pause_for_enter();

        playerData.solitaire.losses++;
        playerData.solitaire.win_streak = 0;
        playerData.total_losses++;
    }

    printf("Final Balance: $%lld\n", playerData.uPlayerMoney);
    pause_for_enter();
    clear_screen();
}

/* ------------------------------------------------------------------------- */
/* Helper functions: dealing, rendering, moves, and simple rules             */
/* ------------------------------------------------------------------------- */

/**
 * deal_new_klondike_game
 * Deal the table in the 1..7 pyramid pattern; last in each column face up.
 * Remaining cards are placed into the draw pile; foundations and waste start empty.
 *
 * @param gameState     Destination game state to initialize.
 * @param shuffledDeck  A 52-card deck (already shuffled by caller).
 */
static void deal_new_klondike_game(KlondikeGame *gameState, Card *shuffledDeck)
{
    int deckReadIndex = 0;

    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        for (int tableRowIndex = 0; tableRowIndex <= tableColumnIndex; ++tableRowIndex)
        {
            gameState->table[tableColumnIndex][tableRowIndex] = shuffledDeck[deckReadIndex++];  /* Place cards into table columns. */
            gameState->table[tableColumnIndex][tableRowIndex].revealed = (tableRowIndex == tableColumnIndex);  /* Only the top card is face-up. */
        }
        gameState->table_counts[tableColumnIndex] = tableColumnIndex + 1;  /* Column sizes are 1..7. */
    }

    /* Stock (draw pile) gets the remainder of the deck. */
    gameState->drawPile.count = 0;

    for (; deckReadIndex < DECK_SIZE; ++deckReadIndex)
    {
        shuffledDeck[deckReadIndex].revealed = 1;  /* Will be face-up when in waste; keep the flag. */
        gameState->drawPile.cards[gameState->drawPile.count++] = shuffledDeck[deckReadIndex];
    }

    /* Foundations start empty. */
    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        gameState->foundation[foundationIndex].count = 0;
    }

    gameState->wastePile.count = 0;  /* Waste starts empty. */
}

/**
 * render_game_ascii
 * Print a simple ASCII snapshot of the current state. Intended for CLI play.
 * (No frame/bounds checking on printf; purely UI.)
 */
static void render_game_ascii(const KlondikeGame *gameState)
{
    clear_screen();
    printf("--- Game View ---\n");

    printf("Draw Pile: %d cards\n", gameState->drawPile.count);

    if (gameState->wastePile.count > 0)
    {
        Card topWasteCard = gameState->wastePile.cards[gameState->wastePile.count - 1];
        printf("Top of Waste: [%s of %s]\n", topWasteCard.rank, topWasteCard.suit);
    }
    else
    {
        printf("Waste Pile: empty\n");
    }

    /* Foundations (if non-empty, show the top card). */
    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        if (gameState->foundation[foundationIndex].count > 0)
        {
            Card topFoundationCard = gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count - 1];
            printf("Foundation %d: [%s of %s]\n", foundationIndex + 1, topFoundationCard.rank, topFoundationCard.suit);
        }
        else
        {
            printf("Foundation %d: empty\n", foundationIndex + 1);
        }
    }

    /* Table columns: show [???] for face-down cards. */
    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        printf("Column %d: ", tableColumnIndex + 1);

        for (int tableRowIndex = 0; tableRowIndex < gameState->table_counts[tableColumnIndex]; ++tableRowIndex)
        {
            const Card *cardPtr = &gameState->table[tableColumnIndex][tableRowIndex];

            if (!cardPtr->revealed)
            {
                printf("[???] ");
            }
            else
            {
                printf("[%s of %s] ", cardPtr->rank, cardPtr->suit);
            }
        }
        printf("\n");
    }
}

/**
 * draw_from_stock
 * Move 1 or 3 cards from stock to waste depending on difficulty. In Easy mode,
 * when the stock is empty and waste is non-empty, recycle waste -> stock and
 * immediately draw again.
 */
static void draw_from_stock(KlondikeGame *gameState)
{
    int drawCount  = (gameState->difficulty == DIFFICULTY_HARD) ? 3 : 1;
    int actualDraw = (gameState->drawPile.count < drawCount) ? gameState->drawPile.count : drawCount;

    if (actualDraw > 0)
    {
        for (int drawIndex = 0; drawIndex < actualDraw; ++drawIndex)
        {
            gameState->wastePile.cards[gameState->wastePile.count++] =
                gameState->drawPile.cards[--gameState->drawPile.count];
        }
    }
    else if (gameState->difficulty == DIFFICULTY_EASY && gameState->wastePile.count > 0)
    {
        /* Easy mode: recycle waste -> draw, then draw again. */
        for (int wasteIndex = gameState->wastePile.count - 1; wasteIndex >= 0; --wasteIndex)
        {
            gameState->drawPile.cards[gameState->drawPile.count++] = gameState->wastePile.cards[wasteIndex];
        }
        gameState->wastePile.count = 0;

        /* Recurse once to perform the draw after recycle. */
        draw_from_stock(gameState);
    }
}

/**
 * is_red_suit
 * Utility to check a card color by suit (Hearts/Diamonds).
 * @return 1 if red, 0 otherwise.
 */
static int is_red_suit(Card card)
{
    return strcmp(card.suit, "Hearts") == 0 || strcmp(card.suit, "Diamonds") == 0;
}

/**
 * is_legal_foundation_placement
 * Enforce foundation rules: card must match suit and be exactly one rank above
 * the current top (or be an Ace into an empty foundation).
 */
static int is_legal_foundation_placement(Card candidateCard, const Stack *foundationStack)
{
    int candidateValue = atoi(candidateCard.rank);  /* "2".."10" */

    if (strcmp(candidateCard.rank, "Ace")   == 0) { candidateValue = 1;  }
    else if (strcmp(candidateCard.rank, "Jack")  == 0) { candidateValue = 11; }
    else if (strcmp(candidateCard.rank, "Queen") == 0) { candidateValue = 12; }
    else if (strcmp(candidateCard.rank, "King")  == 0) { candidateValue = 13; }

    if (foundationStack->count == 0) { return candidateValue == 1; }

    Card topFoundationCard = foundationStack->cards[foundationStack->count - 1];

    int topValue = atoi(topFoundationCard.rank);
    if (strcmp(topFoundationCard.rank, "Ace")   == 0) { topValue = 1;  }
    else if (strcmp(topFoundationCard.rank, "Jack")  == 0) { topValue = 11; }
    else if (strcmp(topFoundationCard.rank, "Queen") == 0) { topValue = 12; }
    else if (strcmp(topFoundationCard.rank, "King")  == 0) { topValue = 13; }

    return (strcmp(candidateCard.suit, topFoundationCard.suit) == 0) && (candidateValue == topValue + 1);
}

/**
 * get_card_rank_value
 * Map ranks to numbers for general comparisons. (Ace=1, Jack=11, Queen=12, King=13)
 */
static int get_card_rank_value(Card card)
{
    if (strcmp(card.rank, "Ace")   == 0) { return 1;  }
    if (strcmp(card.rank, "Jack")  == 0) { return 11; }
    if (strcmp(card.rank, "Queen") == 0) { return 12; }
    if (strcmp(card.rank, "King")  == 0) { return 13; }
    return atoi(card.rank);
}

/**
 * is_legal_table_placement
 * Check whether movingCard can be placed on top of destinationCard in the table:
 * colors must alternate and ranks must be descending by exactly 1.
 */
static int is_legal_table_placement(Card movingCard, Card destinationCard)
{
    int movingValue      = get_card_rank_value(movingCard);
    int destinationValue = get_card_rank_value(destinationCard);

    return (is_red_suit(movingCard) != is_red_suit(destinationCard)) &&
           (movingValue == destinationValue - 1);
}

/* ------------------------------------------------------------------------- */
/* Player move handler                                                        */
/* ------------------------------------------------------------------------- */

/**
 * move_card
 * Player move handler (text UI). Creates an undo snapshot first, then:
 *  1: Waste -> Foundation
 *  2: Waste -> Column (table)
 *  3: Column -> Column (moving a revealed run)
 *  4: Column -> Foundation
 *  5: Foundation -> Column (rare, but supported)
 *  6: Cancel
 *
 * Input validation is minimal; most invalid moves simply do nothing and
 * return to the game loop. This function performs all state mutations for
 * manual moves.
 */
static void move_card(KlondikeGame *gameState)
{
    int userMoveChoice = 0;

    printf("\n");
    printf("1: Waste to foundation\n");
    printf("2: Waste to column\n");
    printf("3: Column to column\n");
    printf("4: Column to foundation\n");
    printf("5: Foundation to column\n");
    printf("6: Cancel\n");
    printf("> ");
    scanf("%d", &userMoveChoice);

    /* Prepare undo snapshot only if a move succeeds; we create the snapshot here
       (prior to any mutation) and keep it if we actually perform a move. */
    snapshot_for_undo(gameState);

    /* 1) Waste -> Foundation (try each foundation in order). */
    if (userMoveChoice == 1 && gameState->wastePile.count > 0)
    {
        Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (is_legal_foundation_placement(wasteTopCard, &gameState->foundation[foundationIndex]))
            {
                gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = wasteTopCard;
                gameState->wastePile.count--;
                return;
            }
        }
    }
    /* 2) Waste -> Column (respect King->empty and descending/alternating otherwise). */
    else if (userMoveChoice == 2 && gameState->wastePile.count > 0)
    {
        Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

        int destColumnNumber = 0;
        printf("To column #: ");
        scanf("%d", &destColumnNumber);
        destColumnNumber--;  /* Convert to 0-based index. */

        if (destColumnNumber >= 0 && destColumnNumber < COLUMNS)
        {
            if (gameState->table_counts[destColumnNumber] == 0 && strcmp(wasteTopCard.rank, "King") == 0)
            {
                gameState->table[destColumnNumber][0] = wasteTopCard;
                gameState->table[destColumnNumber][0].revealed = 1;
                gameState->table_counts[destColumnNumber] = 1;
                gameState->wastePile.count--;
            }
            else if (gameState->table_counts[destColumnNumber] > 0 &&
                     is_legal_table_placement(wasteTopCard, gameState->table[destColumnNumber][gameState->table_counts[destColumnNumber] - 1]))
            {
                gameState->table[destColumnNumber][gameState->table_counts[destColumnNumber]++] = wasteTopCard;
                gameState->wastePile.count--;
            }
        }
    }
    /* 3) Column -> Column (moves a face-up run starting anywhere within the column). */
    else if (userMoveChoice == 3)
    {
        int fromColumnNumber = 0;
        int toColumnNumber   = 0;

        printf("From column #: ");
        scanf("%d", &fromColumnNumber);
        printf("To column #: ");
        scanf("%d", &toColumnNumber);

        fromColumnNumber--;  /* 0-based */
        toColumnNumber--;    /* 0-based */

        if (fromColumnNumber >= 0 && fromColumnNumber < COLUMNS &&
            toColumnNumber   >= 0 && toColumnNumber   < COLUMNS &&
            gameState->table_counts[fromColumnNumber] > 0)
        {
            int firstRevealedRowIndex = -1;

            for (int tableRowIndex = 0; tableRowIndex < gameState->table_counts[fromColumnNumber]; ++tableRowIndex)
            {
                if (gameState->table[fromColumnNumber][tableRowIndex].revealed)
                {
                    firstRevealedRowIndex = tableRowIndex;
                    break;
                }
            }

            if (firstRevealedRowIndex != -1)
            {
                Card *destTopPtr =
                    (gameState->table_counts[toColumnNumber] > 0)
                        ? &gameState->table[toColumnNumber][gameState->table_counts[toColumnNumber] - 1]
                        : NULL;

                for (int splitRowIndex = firstRevealedRowIndex; splitRowIndex < gameState->table_counts[fromColumnNumber]; ++splitRowIndex)
                {
                    if ((destTopPtr == NULL && strcmp(gameState->table[fromColumnNumber][splitRowIndex].rank, "King") == 0) ||
                        (destTopPtr != NULL && is_legal_table_placement(gameState->table[fromColumnNumber][splitRowIndex], *destTopPtr)))
                    {
                        /* Move the entire suffix [splitRowIndex..end]. */
                        int runLength = gameState->table_counts[fromColumnNumber] - splitRowIndex;

                        for (int runOffset = 0; runOffset < runLength; ++runOffset)
                        {
                            gameState->table[toColumnNumber][gameState->table_counts[toColumnNumber]++] =
                                gameState->table[fromColumnNumber][splitRowIndex + runOffset];
                        }

                        gameState->table_counts[fromColumnNumber] = splitRowIndex;

                        if (splitRowIndex > 0)
                        {
                            gameState->table[fromColumnNumber][splitRowIndex - 1].revealed = 1;
                        }
                        return;
                    }
                }

                printf("\nInvalid move: No valid sequence to move.\n");
            }
        }
    }
    /* 4) Column -> Foundation (top card only). */
    else if (userMoveChoice == 4)
    {
        int fromColumnNumber = 0;

        printf("From column #: ");
        scanf("%d", &fromColumnNumber);
        fromColumnNumber--;

        if (fromColumnNumber >= 0 && fromColumnNumber < COLUMNS &&
            gameState->table_counts[fromColumnNumber] > 0 &&
            gameState->table[fromColumnNumber][gameState->table_counts[fromColumnNumber] - 1].revealed)
        {
            Card topTableCard = gameState->table[fromColumnNumber][gameState->table_counts[fromColumnNumber] - 1];

            for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
            {
                if (is_legal_foundation_placement(topTableCard, &gameState->foundation[foundationIndex]))
                {
                    gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = topTableCard;
                    gameState->table_counts[fromColumnNumber]--;

                    if (gameState->table_counts[fromColumnNumber] > 0)
                    {
                        gameState->table[fromColumnNumber][gameState->table_counts[fromColumnNumber] - 1].revealed = 1;
                    }
                    break;
                }
            }
        }
    }
    /* 5) Foundation -> Column (mostly used to unblock moves; rare but legal). */
    else if (userMoveChoice == 5)
    {
        int fromFoundationNumber = 0;
        int toColumnNumber       = 0;

        printf("From foundation #: ");
        scanf("%d", &fromFoundationNumber);
        printf("To column #: ");
        scanf("%d", &toColumnNumber);

        fromFoundationNumber--;
        toColumnNumber--;

        if (fromFoundationNumber >= 0 && fromFoundationNumber < FOUNDATION_PILES &&
            gameState->foundation[fromFoundationNumber].count > 0 &&
            toColumnNumber >= 0 && toColumnNumber < COLUMNS)
        {
            Card foundationTopCard = gameState->foundation[fromFoundationNumber].cards[gameState->foundation[fromFoundationNumber].count - 1];

            if ((gameState->table_counts[toColumnNumber] == 0 && strcmp(foundationTopCard.rank, "King") == 0) ||
                (gameState->table_counts[toColumnNumber] > 0 &&
                 is_legal_table_placement(foundationTopCard, gameState->table[toColumnNumber][gameState->table_counts[toColumnNumber] - 1])))
            {
                foundationTopCard.revealed = 1;
                gameState->table[toColumnNumber][gameState->table_counts[toColumnNumber]++] = foundationTopCard;
                gameState->foundation[fromFoundationNumber].count--;
            }
        }
    }
    else
    {
        printf("\nMove canceled.\n");
    }
}

/* -------------------------------------------------------------------------- */
/* Main interactive loop                                                      */
/* -------------------------------------------------------------------------- */

/**
 * run_game_loop
 * Main interactive loop for the game session. Renders the state, collects user
 * input, and applies moves (including draw and optional auto-complete).
 *
 * @param gameState  In/out game state (mutated as player plays).
 * @param betAmount  Bet amount (0 in Easy).
 * @return true on a win, false otherwise (quit or loss).
 */
static bool run_game_loop(KlondikeGame *gameState, unsigned int betAmount)
{
    while (1)
    {
        render_game_ascii(gameState);

        printf("\nOptions:\n");
        printf("1: Draw card\n");
        printf("2: Move card\n");

        int autoCompleteMenuNumber = 0;

        if (gameState->difficulty == DIFFICULTY_EASY)
        {
            printf("3: Undo move\n");
            printf("4: Quit game\n");
            autoCompleteMenuNumber = 5;
        }
        else
        {
            printf("3: Quit game\n");
            autoCompleteMenuNumber = 4;
        }

        if (is_safe_to_auto_complete(gameState))
        {
            printf("%d: Auto Complete\n", autoCompleteMenuNumber);
        }

        printf("> ");

        int userActionChoice = 0;
        scanf("%d", &userActionChoice);

        if (userActionChoice == 1)
        {
            snapshot_for_undo(gameState);
            draw_from_stock(gameState);
        }
        else if (userActionChoice == 2)
        {
            move_card(gameState);
        }
        else if (userActionChoice == 3 && gameState->difficulty == DIFFICULTY_EASY)
        {
            restore_undo(gameState);
            gameState->undo = true;  /* Mark that undo was used (affects stats like perfect clears). */
        }
        else if ((gameState->difficulty == DIFFICULTY_EASY && userActionChoice == 4) ||
                 (gameState->difficulty != DIFFICULTY_EASY && userActionChoice == 3))
        {
            /* Quit path: offer to save and then report loss. */
            save_prompt(gameState);

            printf("\nGame over. You did not complete all foundations.\n");

            if (gameState->difficulty == DIFFICULTY_NORMAL)
            {
                printf("You lose your bet of $%u.\n", betAmount);
            }
            else if (gameState->difficulty == DIFFICULTY_HARD)
            {
                printf("You lose your bet of $%u.\n", betAmount);
            }

            pause_for_enter();

            playerData.solitaire.losses++;
            playerData.solitaire.win_streak = 0;
            playerData.total_losses++;

            solitaire();  /* Return to main menu */
            return false;
        }
        else if (is_safe_to_auto_complete(gameState) && userActionChoice == autoCompleteMenuNumber)
        {
            /* Auto-complete only when heuristically “safe enough” (see is_safe_to_auto_complete). */
            perform_auto_complete(gameState);

            printf("\nAuto Complete finished! You win!\n");

            if (gameState->difficulty == DIFFICULTY_NORMAL)
            {
                playerData.uPlayerMoney += betAmount * 2U;
                printf("You win! Earned 2x your bet: $%u\n", betAmount * 2U);
                playerData.solitaire.normal_wins++;
                return true;
            }
            else if (gameState->difficulty == DIFFICULTY_HARD)
            {
                playerData.uPlayerMoney += betAmount * 5U;
                printf("You win! Earned 5x your bet: $%u\n", betAmount * 5U);
                playerData.solitaire.hard_wins++;
                return true;
            }
            else
            {
                printf("You win! (Easy Mode).\n");
                playerData.solitaire.easy_wins++;
                return true;
            }

            /* (Dead code path, retained for clarity of original design.) */
            playerData.solitaire.wins++;
            if (playerData.solitaire.max_win_streak <= playerData.solitaire.win_streak)
            {
                playerData.solitaire.max_win_streak = playerData.solitaire.win_streak;
            }
            playerData.games_played++;
            playerData.total_wins++;
            if (gameState->undo) { playerData.solitaire.perfect_clear++; }
            checkAchievements();
            save_player_data();
            save_achievements();
        }

        /* Manual win detection (if the player achieves goal without auto-complete). */
        int numCompleteFoundations = 0;

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (gameState->foundation[foundationIndex].count == MAX_FOUNDATION) { ++numCompleteFoundations; }
        }

        if (numCompleteFoundations == FOUNDATION_PILES) { return true; }
    }

    /* Unreached */
    /* return false; */
}

/* -------------------------------------------------------------------------- */
/* Auto-complete + gating heuristic                                           */
/* -------------------------------------------------------------------------- */

/**
 * perform_auto_complete
 * Repeatedly push everything that can go to foundations (table then waste)
 * until no further pushes are possible. This ignores deeper strategy and is
 * intended only when the position is clearly “clean-up” solvable.
 */
static void perform_auto_complete(KlondikeGame *gameState)
{
    int didMoveThisPass = 0;

    do
    {
        didMoveThisPass = 0;

        /* Table -> foundation (tops only, if revealed & legal). */
        for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
        {
            if (gameState->table_counts[tableColumnIndex] > 0)
            {
                Card topTableCard = gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1];

                if (topTableCard.revealed)
                {
                    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
                    {
                        if (is_legal_foundation_placement(topTableCard, &gameState->foundation[foundationIndex]))
                        {
                            gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = topTableCard;
                            gameState->table_counts[tableColumnIndex]--;

                            if (gameState->table_counts[tableColumnIndex] > 0)
                            {
                                gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1].revealed = 1;
                            }

                            didMoveThisPass = 1;
                            break;
                        }
                    }
                }
            }
        }

        /* Waste -> foundation (top only). */
        if (gameState->wastePile.count > 0)
        {
            Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

            for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
            {
                if (is_legal_foundation_placement(wasteTopCard, &gameState->foundation[foundationIndex]))
                {
                    gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = wasteTopCard;
                    gameState->wastePile.count--;
                    didMoveThisPass = 1;
                    break;
                }
            }
        }
    }
    while (didMoveThisPass);
}

/**
 * is_safe_to_auto_complete
 * Heuristic gate for the auto-complete option:
 *  - all table cards must be face up (no hidden blockers),
 *  - each foundation must be at least at rank 5.
 * These thresholds are conservative and ensure auto-complete won't get stuck.
 */
static int is_safe_to_auto_complete(const KlondikeGame *gameState)
{
    /* All table cards must be revealed. */
    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        for (int tableRowIndex = 0; tableRowIndex < gameState->table_counts[tableColumnIndex]; ++tableRowIndex)
        {
            if (!gameState->table[tableColumnIndex][tableRowIndex].revealed) { return 0; }
        }
    }

    /* Each foundation must be at least at rank 5. */
    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        if (gameState->foundation[foundationIndex].count == 0) { return 0; }

        Card topFoundationCard = gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count - 1];
        int  topValue          = get_card_rank_value(topFoundationCard);

        if (topValue < 5) { return 0; }
    }

    return 1;
}

/* ------------------------------------------------------------------------- */
/* DFS / Backtracking Solver (heap-backed states + transposition + pruning)   */
/* ------------------------------------------------------------------------- */

/* Small helper: 64-bit rotate-left (used by the hasher). */
static uint64_t rotl64_u(uint64_t value64, int rotateBits)
{
    return (value64 << rotateBits) | (value64 >> (64 - rotateBits));
}

/* --- Hashing helpers to create a stable, compact key for a game state --- */

/* Map rank/suit strings to small integers for hash mixing. */
static int encode_rank_to_id(const char *rankStr)
{
    if (!strcmp(rankStr, "Ace"))   { return 1;  }
    if (!strcmp(rankStr, "Jack"))  { return 11; }
    if (!strcmp(rankStr, "Queen")) { return 12; }
    if (!strcmp(rankStr, "King"))  { return 13; }
    return atoi(rankStr);  /* "2".."10" */
}
static int encode_suit_to_id(const char *suitStr)
{
    if (!strcmp(suitStr, "Hearts"))   { return 0; }
    if (!strcmp(suitStr, "Diamonds")) { return 1; }
    if (!strcmp(suitStr, "Clubs"))    { return 2; }
    if (!strcmp(suitStr, "Spades"))   { return 3; }
    return 0; /* fallback */
}

/* Lightly mixed per-card hash (includes revealed flag). */
static uint64_t compute_card_hash(Card card)
{
    uint64_t mixedValue = (uint64_t)(encode_rank_to_id(card.rank) & 0x3F) |
                          ((uint64_t)(encode_suit_to_id(card.suit) & 0x03) << 6);

    mixedValue |= ((uint64_t)(card.revealed ? 1 : 0) << 8);
    mixedValue ^= rotl64_u(mixedValue * 0x9E3779B185EBCA87ULL + 0xC2B2AE3D27D4EB4FULL, 23);
    return mixedValue;
}

/* State hash: foundations (top+count), full table arrays (up to counts),
 * waste sequence, draw sequence, and difficulty. */
static uint64_t compute_state_hash(const KlondikeGame *gameState)
{
    uint64_t stateHash = 0xDEADBEEFCAFEBABEULL;

    /* Foundations: count and top card carry most of the signal. */
    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        stateHash ^= (uint64_t)gameState->foundation[foundationIndex].count + 0x9E37;

        if (gameState->foundation[foundationIndex].count > 0)
        {
            Card topFoundationCard = gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count - 1];
            stateHash ^= rotl64_u(compute_card_hash(topFoundationCard), foundationIndex + 1);
        }
    }

    /* Table: mix in all present cards in each column. */
    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        stateHash ^= rotl64_u((uint64_t)gameState->table_counts[tableColumnIndex] + 0x12D + tableColumnIndex, 7);

        for (int cardIndex = 0; cardIndex < gameState->table_counts[tableColumnIndex]; ++cardIndex)
        {
            stateHash ^= rotl64_u(compute_card_hash(gameState->table[tableColumnIndex][cardIndex]),
                                  (cardIndex + tableColumnIndex) & 31);
        }
    }

    /* Waste sequence (from bottom to top). */
    stateHash ^= rotl64_u((uint64_t)gameState->wastePile.count + 0x55, 13);
    for (int cardIndex = 0; cardIndex < gameState->wastePile.count; ++cardIndex)
    {
        stateHash ^= rotl64_u(compute_card_hash(gameState->wastePile.cards[cardIndex]), cardIndex & 31);
    }

    /* Draw sequence. */
    stateHash ^= rotl64_u((uint64_t)gameState->drawPile.count + 0xA3, 17);
    for (int cardIndex = 0; cardIndex < gameState->drawPile.count; ++cardIndex)
    {
        stateHash ^= rotl64_u(compute_card_hash(gameState->drawPile.cards[cardIndex]), cardIndex & 31);
    }

    /* Include difficulty since it changes stock cycling. */
    stateHash ^= (uint64_t)gameState->difficulty * 0x1000193ULL;

    return stateHash;
}

/* Transposition table probes (linear probing with short probe cap for speed). */
static int visited_table_contains(VisitedEntry *visitedTable, uint64_t stateKey)
{
    uint64_t startSlot = stateKey % VISITED_CAP;

    for (int probeStep = 0; probeStep < 16; ++probeStep)
    {
        uint64_t tableIndex = (startSlot + probeStep) % VISITED_CAP;

        if (!visitedTable[tableIndex].used) { return 0; }
        if (visitedTable[tableIndex].key == stateKey) { return 1; }
    }
    return 0;
}
static void visited_table_insert(VisitedEntry *visitedTable, uint64_t stateKey)
{
    uint64_t startSlot = stateKey % VISITED_CAP;

    for (int probeStep = 0; probeStep < VISITED_CAP; ++probeStep)
    {
        uint64_t tableIndex = (startSlot + probeStep) % VISITED_CAP;

        if (!visitedTable[tableIndex].used)
        {
            visitedTable[tableIndex].used = 1;
            visitedTable[tableIndex].key  = stateKey;
            return;
        }
        if (visitedTable[tableIndex].key == stateKey) { return; }
    }
}

/* --- Small utilities used by both gameplay and solver --- */

/* Reveal the new top of a table column after cards were removed. */
static inline void reveal_new_table_top_card(KlondikeGame *gameState, int tableColumnIndex)
{
    if (gameState->table_counts[tableColumnIndex] > 0)
    {
        gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1].revealed = 1;
    }
}

/* Try to move sequence starting at index `startRowIndex` in column `fromColumnIndex` onto column `toColumnIndex`.
 * In addition to normal legality, we require that the destination has room. */
static int can_move_sequence_onto_column(const KlondikeGame *gameState, int fromColumnIndex, int startRowIndex, int toColumnIndex)
{
    int moveCount = gameState->table_counts[fromColumnIndex] - startRowIndex;
    if (moveCount <= 0) { return 0; }

    /* Capacity guard for destination column. */
    if (gameState->table_counts[toColumnIndex] + moveCount > MAX_DRAW_STACK) { return 0; }

    if (gameState->table_counts[toColumnIndex] == 0)
    {
        return strcmp(gameState->table[fromColumnIndex][startRowIndex].rank, "King") == 0;
    }
    else
    {
        Card destTopCard = gameState->table[toColumnIndex][gameState->table_counts[toColumnIndex] - 1];
        return is_legal_table_placement(gameState->table[fromColumnIndex][startRowIndex], destTopCard);
    }
}

/* Apply table-sequence move with capacity guard. */
static void apply_move_sequence_between_columns(KlondikeGame *gameState, int fromColumnIndex, int startRowIndex, int toColumnIndex)
{
    int moveCount = gameState->table_counts[fromColumnIndex] - startRowIndex;
    if (moveCount <= 0) { return; }

    /* Destination capacity check (defensive; should already be filtered). */
    if (gameState->table_counts[toColumnIndex] + moveCount > MAX_DRAW_STACK) { return; }

    for (int j = 0; j < moveCount; ++j)
    {
        gameState->table[toColumnIndex][gameState->table_counts[toColumnIndex]++] =
            gameState->table[fromColumnIndex][startRowIndex + j];
    }

    gameState->table_counts[fromColumnIndex] = startRowIndex;

    if (gameState->table_counts[fromColumnIndex] > 0)
    {
        gameState->table[fromColumnIndex][gameState->table_counts[fromColumnIndex] - 1].revealed = 1;
    }
}

/* Goal check: all four foundations must be complete. */
static inline int is_goal_state(const KlondikeGame *gameState)
{
    int completeCount = 0;

    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        if (gameState->foundation[foundationIndex].count == MAX_FOUNDATION) { ++completeCount; }
    }

    return completeCount == FOUNDATION_PILES;
}

/* -------------------------------------------------------------------------- */
/* Pruning / Forced-move heuristics (shared with solver)                      */
/* -------------------------------------------------------------------------- */

/**
 * max_foundation_rank_by_color
 * Compute the highest rank currently placed on foundations for red suits
 * (Hearts/Diamonds) and black suits (Clubs/Spades). Used by safe-to-foundation.
 */
static void max_foundation_rank_by_color(const KlondikeGame *gameState, int *maxRedOut, int *maxBlackOut)
{
    int maxRedLocal   = 0;
    int maxBlackLocal = 0;

    for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
    {
        if (gameState->foundation[foundationIndex].count == 0) { continue; }

        Card topFoundationCard = gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count - 1];
        int  rankValue         = get_card_rank_value(topFoundationCard);

        if (is_red_suit(topFoundationCard))
        {
            if (rankValue > maxRedLocal)   { maxRedLocal   = rankValue; }
        }
        else
        {
            if (rankValue > maxBlackLocal) { maxBlackLocal = rankValue; }
        }
    }

    *maxRedOut   = maxRedLocal;
    *maxBlackOut = maxBlackLocal;
}

/**
 * is_safe_foundation_push
 * Classic heuristic: moving Ace/Two is always safe. For higher cards of value v:
 *  - Only move a red card if the max black foundation is at least v-1.
 *  - Only move a black card if the max red foundation is at least v-1.
 * This prevents “locking” low cards on foundation when they are still needed
 * to build sequences in the table.
 */
static int is_safe_foundation_push(Card candidateCard, const KlondikeGame *gameState)
{
    int rankValue = get_card_rank_value(candidateCard);
    if (rankValue <= 2) { return 1; }  /* A, 2 are always safe. */

    int maxRed   = 0;
    int maxBlack = 0;

    max_foundation_rank_by_color(gameState, &maxRed, &maxBlack);

    if (is_red_suit(candidateCard)) { return maxBlack >= (rankValue - 1); }
    else                             { return maxRed   >= (rankValue - 1); }
}

/**
 * exists_any_table_to_table_move
 * Lightweight “is there any legal table move?” predicate.
 * Helps prune dead branches quickly in the solver.
 */
static int exists_any_table_to_table_move(const KlondikeGame *gameState)
{
    for (int fromColumnIndex = 0; fromColumnIndex < COLUMNS; ++fromColumnIndex)
    {
        int columnCount = gameState->table_counts[fromColumnIndex];
        if (columnCount == 0) { continue; }

        int firstRevealedRowIndex = -1;

        for (int tableRowIndex = 0; tableRowIndex < columnCount; ++tableRowIndex)
        {
            if (gameState->table[fromColumnIndex][tableRowIndex].revealed)
            {
                firstRevealedRowIndex = tableRowIndex;
                break;
            }
        }

        if (firstRevealedRowIndex == -1) { continue; }

        for (int splitRowIndex = firstRevealedRowIndex; splitRowIndex < columnCount; ++splitRowIndex)
        {
            /* Ensure the subsequence is valid (descending, alternating colors). */
            int isValidSequence = 1;

            for (int checkIndex = splitRowIndex; checkIndex < columnCount - 1; ++checkIndex)
            {
                if (!is_legal_table_placement(gameState->table[fromColumnIndex][checkIndex + 1],
                                              gameState->table[fromColumnIndex][checkIndex]))
                {
                    isValidSequence = 0;
                    break;
                }
            }

            if (!isValidSequence) { continue; }

            for (int toColumnIndex = 0; toColumnIndex < COLUMNS; ++toColumnIndex)
            {
                if (toColumnIndex == fromColumnIndex) { continue; }

                if (gameState->table_counts[toColumnIndex] == 0)
                {
                    if (strcmp(gameState->table[fromColumnIndex][splitRowIndex].rank, "King") == 0) { return 1; }
                }
                else
                {
                    Card destTopCard = gameState->table[toColumnIndex][gameState->table_counts[toColumnIndex] - 1];
                    if (is_legal_table_placement(gameState->table[fromColumnIndex][splitRowIndex], destTopCard)) { return 1; }
                }
            }
        }
    }
    return 0;
}

/**
 * exists_any_waste_to_table_move
 * Quick predicate for whether the waste top can be placed on any column.
 */
static int exists_any_waste_to_table_move(const KlondikeGame *gameState)
{
    if (gameState->wastePile.count == 0) { return 0; }

    Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

    for (int toColumnIndex = 0; toColumnIndex < COLUMNS; ++toColumnIndex)
    {
        if (gameState->table_counts[toColumnIndex] == 0)
        {
            if (strcmp(wasteTopCard.rank, "King") == 0) { return 1; }
        }
        else
        {
            Card destTopCard = gameState->table[toColumnIndex][gameState->table_counts[toColumnIndex] - 1];
            if (is_legal_table_placement(wasteTopCard, destTopCard)) { return 1; }
        }
    }
    return 0;
}

/**
 * exists_any_safe_foundation_push
 * True if *any* safe-to-foundation move exists from table or waste.
 */
static int exists_any_safe_foundation_push(const KlondikeGame *gameState)
{
    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        if (gameState->table_counts[tableColumnIndex] == 0) { continue; }

        Card topTableCard = gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1];
        if (!topTableCard.revealed) { continue; }

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (is_legal_foundation_placement(topTableCard, &gameState->foundation[foundationIndex]) &&
                is_safe_foundation_push(topTableCard, gameState))
            {
                return 1;
            }
        }
    }

    if (gameState->wastePile.count > 0)
    {
        Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (is_legal_foundation_placement(wasteTopCard, &gameState->foundation[foundationIndex]) &&
                is_safe_foundation_push(wasteTopCard, gameState))
            {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * exists_any_progress_move
 * Coarse pruning gate for the solver:
 *  - Any safe-to-foundation push?
 *  - Any table->table move?
 *  - Any waste->table move?
 *  - Else, can we draw (or recycle in Easy)?
 *  - Otherwise: dead leaf (return 0).
 */
static int exists_any_progress_move(const KlondikeGame *gameState)
{
    if (exists_any_safe_foundation_push(gameState)) { return 1; }
    if (exists_any_table_to_table_move(gameState))  { return 1; }
    if (exists_any_waste_to_table_move(gameState))  { return 1; }

    if (gameState->drawPile.count > 0) { return 1; }
    if (gameState->difficulty == DIFFICULTY_EASY && gameState->wastePile.count > 0) { return 1; }

    return 0;
}

/**
 * apply_forced_moves
 * Collapse “obvious” safe foundation moves repeatedly until none apply.
 * This reduces branching and tends to expose more revealed table cards.
 *
 * @return 1 if the position changed at least once, 0 otherwise.
 */
static int apply_forced_moves(KlondikeGame *gameState)
{
    int changedAny      = 0;
    int changedThisPass = 0;

    do
    {
        changedThisPass = 0;

        /* Table -> foundation (safe only). */
        for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
        {
            if (gameState->table_counts[tableColumnIndex] == 0) { continue; }

            Card topTableCard = gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1];
            if (!topTableCard.revealed) { continue; }

            for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
            {
                if (is_legal_foundation_placement(topTableCard, &gameState->foundation[foundationIndex]) &&
                    is_safe_foundation_push(topTableCard, gameState))
                {
                    gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = topTableCard;
                    gameState->table_counts[tableColumnIndex]--;

                    if (gameState->table_counts[tableColumnIndex] > 0)
                    {
                        gameState->table[tableColumnIndex][gameState->table_counts[tableColumnIndex] - 1].revealed = 1;
                    }

                    changedThisPass = 1;
                    changedAny      = 1;
                    break;
                }
            }
        }

        /* Waste -> foundation (safe only). */
        if (!changedThisPass && gameState->wastePile.count > 0)
        {
            Card wasteTopCard = gameState->wastePile.cards[gameState->wastePile.count - 1];

            for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
            {
                if (is_legal_foundation_placement(wasteTopCard, &gameState->foundation[foundationIndex]) &&
                    is_safe_foundation_push(wasteTopCard, gameState))
                {
                    gameState->foundation[foundationIndex].cards[gameState->foundation[foundationIndex].count++] = wasteTopCard;
                    gameState->wastePile.count--;

                    changedThisPass = 1;
                    changedAny      = 1;
                    break;
                }
            }
        }
    }
    while (changedThisPass);

    return changedAny;
}

/* -------------------------------------------------------------------------- */
/* DFS search engine                                                          */
/* -------------------------------------------------------------------------- */

/* Global node counter (guard against runaway searches). */
static size_t g_DfsNodeCount = 0;

/**
 * dfs_search_inner
 * Core recursive DFS over a heap-allocated “states” array:
 *   - states[depth] is the current node,
 *   - children are written into states[depth+1] before recursing,
 *   - we apply forced moves first and prune via transposition + “no progress”.
 *
 * @param statesArray   Pre-allocated array of size DFS_MAX_DEPTH+2.
 * @param searchDepth   Current search depth (0-based).
 * @param visitedTable  Transposition table (may be NULL to disable).
 * @return 1 if a winning line is found, 0 otherwise.
 */
static int dfs_search_inner(KlondikeGame *statesArray, int searchDepth, VisitedEntry *visitedTable)
{
    if (searchDepth >= DFS_MAX_DEPTH) { return 0; }

    KlondikeGame *currentState = &statesArray[searchDepth];

    /* Apply safe/forced moves in-place to shrink branching. */
    apply_forced_moves(currentState);

    if (is_goal_state(currentState)) { return 1; }

    /* Node budget (hard cap). */
    if (++g_DfsNodeCount > DFS_NODE_LIMIT) { return 0; }

    /* Transposition table guard. */
    uint64_t stateKey = compute_state_hash(currentState);

    if (visitedTable && visited_table_contains(visitedTable, stateKey)) { return 0; }
    if (visitedTable) { visited_table_insert(visitedTable, stateKey); }

    /* Quick prune (no moves & no draw/recycle). */
    if (!exists_any_progress_move(currentState)) { return 0; }

    /* 1) Table top -> foundation (safe only). */
    for (int tableColumnIndex = 0; tableColumnIndex < COLUMNS; ++tableColumnIndex)
    {
        if (currentState->table_counts[tableColumnIndex] == 0) { continue; }

        Card topTableCard = currentState->table[tableColumnIndex][currentState->table_counts[tableColumnIndex] - 1];
        if (!topTableCard.revealed) { continue; }

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (is_legal_foundation_placement(topTableCard, &currentState->foundation[foundationIndex]) &&
                is_safe_foundation_push(topTableCard, currentState))
            {
                statesArray[searchDepth + 1] = *currentState;

                KlondikeGame *nextState = &statesArray[searchDepth + 1];
                nextState->foundation[foundationIndex].cards[nextState->foundation[foundationIndex].count++] = topTableCard;
                nextState->table_counts[tableColumnIndex]--;

                reveal_new_table_top_card(nextState, tableColumnIndex);

                if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
            }
        }
    }

    /* 2) Waste -> foundation (safe only). */
    if (currentState->wastePile.count > 0)
    {
        Card wasteTopCard = currentState->wastePile.cards[currentState->wastePile.count - 1];

        for (int foundationIndex = 0; foundationIndex < FOUNDATION_PILES; ++foundationIndex)
        {
            if (is_legal_foundation_placement(wasteTopCard, &currentState->foundation[foundationIndex]) &&
                is_safe_foundation_push(wasteTopCard, currentState))
            {
                statesArray[searchDepth + 1] = *currentState;

                KlondikeGame *nextState = &statesArray[searchDepth + 1];
                nextState->foundation[foundationIndex].cards[nextState->foundation[foundationIndex].count++] = wasteTopCard;

                if (nextState->wastePile.count > 0) { nextState->wastePile.count--; }

                if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
            }
        }
    }

    /* 3) Table -> table (move any legal revealed sequence). */
    for (int fromColumnIndex = 0; fromColumnIndex < COLUMNS; ++fromColumnIndex)
    {
        int columnCount = currentState->table_counts[fromColumnIndex];
        if (columnCount == 0) { continue; }

        int firstRevealedRowIndex = -1;

        for (int tableRowIndex = 0; tableRowIndex < columnCount; ++tableRowIndex)
        {
            if (currentState->table[fromColumnIndex][tableRowIndex].revealed)
            {
                firstRevealedRowIndex = tableRowIndex;
                break;
            }
        }

        if (firstRevealedRowIndex == -1) { continue; }

        for (int splitRowIndex = firstRevealedRowIndex; splitRowIndex < columnCount; ++splitRowIndex)
        {
            int isValidSequence = 1;

            for (int checkIndex = splitRowIndex; checkIndex < columnCount - 1; ++checkIndex)
            {
                if (!is_legal_table_placement(currentState->table[fromColumnIndex][checkIndex + 1],
                                              currentState->table[fromColumnIndex][checkIndex]))
                {
                    isValidSequence = 0;
                    break;
                }
            }

            if (!isValidSequence) { continue; }

            for (int toColumnIndex = 0; toColumnIndex < COLUMNS; ++toColumnIndex)
            {
                if (toColumnIndex == fromColumnIndex) { continue; }

                if (!can_move_sequence_onto_column(currentState, fromColumnIndex, splitRowIndex, toColumnIndex)) { continue; }

                statesArray[searchDepth + 1] = *currentState;

                KlondikeGame *nextState = &statesArray[searchDepth + 1];
                apply_move_sequence_between_columns(nextState, fromColumnIndex, splitRowIndex, toColumnIndex);

                if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
            }
        }
    }

    /* 4) Waste -> table (place King to empty or descending/alt-color otherwise). */
    if (currentState->wastePile.count > 0)
    {
        Card wasteTopCard = currentState->wastePile.cards[currentState->wastePile.count - 1];

        for (int toColumnIndex = 0; toColumnIndex < COLUMNS; ++toColumnIndex)
        {
            if (currentState->table_counts[toColumnIndex] == 0)
            {
                if (strcmp(wasteTopCard.rank, "King") != 0) { continue; }

                statesArray[searchDepth + 1] = *currentState;

                KlondikeGame *nextState = &statesArray[searchDepth + 1];
                wasteTopCard.revealed = 1;
                nextState->table[toColumnIndex][nextState->table_counts[toColumnIndex]++] = wasteTopCard;

                if (nextState->wastePile.count > 0) { nextState->wastePile.count--; }

                if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
            }
            else
            {
                Card destTopCard = currentState->table[toColumnIndex][currentState->table_counts[toColumnIndex] - 1];

                if (is_legal_table_placement(wasteTopCard, destTopCard))
                {
                    statesArray[searchDepth + 1] = *currentState;

                    KlondikeGame *nextState = &statesArray[searchDepth + 1];
                    nextState->table[toColumnIndex][nextState->table_counts[toColumnIndex]++] = wasteTopCard;

                    if (nextState->wastePile.count > 0) { nextState->wastePile.count--; }

                    if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
                }
            }
        }
    }

    /* 5) Draw from stock (and recycle in Easy). */
    {
        int drawCount = (currentState->difficulty == DIFFICULTY_HARD) ? 3 : 1;

        if (currentState->drawPile.count > 0)
        {
            statesArray[searchDepth + 1] = *currentState;

            KlondikeGame *nextState = &statesArray[searchDepth + 1];
            int actualDraw = (nextState->drawPile.count < drawCount) ? nextState->drawPile.count : drawCount;

            for (int drawIndex = 0; drawIndex < actualDraw; ++drawIndex)
            {
                nextState->wastePile.cards[nextState->wastePile.count++] =
                    nextState->drawPile.cards[--nextState->drawPile.count];
            }

            if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
        }
        else if (currentState->difficulty == DIFFICULTY_EASY && currentState->wastePile.count > 0)
        {
            /* Easy: recycle waste -> draw and keep searching. */
            statesArray[searchDepth + 1] = *currentState;

            KlondikeGame *nextState = &statesArray[searchDepth + 1];

            for (int wasteIndex = nextState->wastePile.count - 1; wasteIndex >= 0; --wasteIndex)
            {
                nextState->drawPile.cards[nextState->drawPile.count++] = nextState->wastePile.cards[wasteIndex];
            }

            nextState->wastePile.count = 0;

            if (dfs_search_inner(statesArray, searchDepth + 1, visitedTable)) { return 1; }
        }
    }

    return 0;
}

/**
 * dfs_solitaire_win
 * Entry point for the solver. Allocates:
 *  - a transposition table (VisitedEntry[]),
 *  - a heap array of states sized by DFS_MAX_DEPTH (prevents stack overflow),
 * and invokes dfs_search_inner().
 *
 * @return true if a winning sequence was found from the initial state.
 */
bool dfs_solitaire_win(KlondikeGame *gameState)
{
    if (is_goal_state(gameState)) { return true; }

    VisitedEntry *visitedTable = (VisitedEntry *)calloc(VISITED_CAP, sizeof(VisitedEntry));
    if (!visitedTable)
    {
        /* If we can't allocate, fail gracefully (no crash). */
        return false;
    }

    KlondikeGame *statesArray = (KlondikeGame *)malloc(sizeof(KlondikeGame) * (DFS_MAX_DEPTH + 2));
    if (!statesArray)
    {
        free(visitedTable);
        return false;
    }

    statesArray[0] = *gameState;

    g_DfsNodeCount = 0;

    int dfsResult = dfs_search_inner(statesArray, 0, visitedTable);

    free(statesArray);
    free(visitedTable);

    return dfsResult ? true : false;
}
