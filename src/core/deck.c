/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Core deck & persistence utilities.
 *
 * Responsibilities:
 *   - Save/load PlayerData + GameConfig as a single binary blob.
 *   - 52-card deck helpers (initialize, shuffle, print).
 *   - Small CLI helpers (clear_screen, pause_for_enter).
 */

#include "core.h"
#include "paths.h"

/* ------------------------------------------------------------------------- */
/* Local tables (rank/suit strings)                                          */
/* ------------------------------------------------------------------------- */

static const char *suits[NUM_SUITS] = {"Hearts", "Diamonds", "Clubs", "Spades"};
static const char *ranks[NUM_RANKS] = {"2","3","4","5","6","7","8","9","10","Jack","Queen","King","Ace"};

/* ------------------------------------------------------------------------- */
/* Persistence                                                               */
/* ------------------------------------------------------------------------- */

/**
 * save_player_data
 * Persist both PlayerData and GameConfig in a single binary blob.
 *
 * @return true on success, false on error opening/writing the file.
 */
bool save_player_data(void)
{
    FILE *fp = fopen(PLAYER_DATA_PATH, "wb");
    if (!fp) return false;

    const size_t w1 = fwrite(&playerData, sizeof(PlayerData), 1, fp);
    const size_t w2 = fwrite(&config,     sizeof(GameConfig), 1, fp);
    fclose(fp);

    return (w1 == 1 && w2 == 1);
}

/**
 * load_player_data
 * Load PlayerData + GameConfig if present; caller should normalize values.
 *
 * @return true on success (file found + read ok), false otherwise.
 */
bool load_player_data(void)
{
    FILE *fp = fopen(PLAYER_DATA_PATH, "rb");
    if (!fp) return false;

    const size_t r1 = fread(&playerData, sizeof(PlayerData), 1, fp);
    const size_t r2 = fread(&config,     sizeof(GameConfig), 1, fp);
    fclose(fp);

    return (r1 == 1 && r2 == 1);
}

/* ------------------------------------------------------------------------- */
/* Deck helpers                                                              */
/* ------------------------------------------------------------------------- */

/**
 * initialize_deck
 * Populate a 52-card deck in suit-major order (no jokers here).
 *
 * @param deck  Out array sized DECK_SIZE.
 */
void initialize_deck(Card *deck)
{
    int cardWriteIndex = 0;
    for (int suitIndex = 0; suitIndex < NUM_SUITS; ++suitIndex) {
        for (int rankIndex = 0; rankIndex < NUM_RANKS; ++rankIndex) {
            deck[cardWriteIndex].suit     = (char*)suits[suitIndex];
            deck[cardWriteIndex].rank     = (char*)ranks[rankIndex];
            deck[cardWriteIndex].revealed = 0;
            deck[cardWriteIndex].is_joker = false;
            ++cardWriteIndex;
        }
    }
}

/**
 * shuffle_deck
 * In-place Fisher–Yates shuffle using rand().
 */
void shuffle_deck(Card *deck)
{
    for (int cardIndex = DECK_SIZE - 1; cardIndex > 0; --cardIndex) {
        const int swapIndex = rand() % (cardIndex + 1);
        Card tmp            = deck[cardIndex];
        deck[cardIndex]     = deck[swapIndex];
        deck[swapIndex]     = tmp;
    }
}

/**
 * print_deck
 * Debug helper: dump the 52-card deck as "Rank of Suit".
 */
void print_deck(Card *deck)
{
    for (int i = 0; i < DECK_SIZE; ++i) {
        printf("%s of %s\n", deck[i].rank, deck[i].suit);
    }
    fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* CLI helpers                                                               */
/* ------------------------------------------------------------------------- */

/**
 * clear_screen
 * Cross-platform clear (crude, but fine for a CLI game).
 */
void clear_screen(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/**
 * pause_for_enter
 * Pause until the user presses Enter (used by menus).
 * Reads and discards a pending newline from prior scanf, then waits for Enter.
 */
void pause_for_enter(void)
{
    printf("Press Enter to continue...");
    int c = getchar(); (void)c; /* swallow any leftover newline */
    getchar();
}