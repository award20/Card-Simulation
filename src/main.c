/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 *
 * Program entry + global menus, persistence, and housekeeping utilities.
 *
 * Responsibilities:
 *   - Initialize player/config data and normalize persisted values.
 *   - Start/stop background threads (time tracking, optional autosave).
 *   - Provide top-level menus (main, games, rules, other) and invoke games.
 *   - Deck helpers (init, shuffle, print) shared across game modules.
 */

#include "core.h"
#include "paths.h"

#ifdef _WIN32
  #include <windows.h>                  /* Threads + Sleep.                   */
#else
  /* Non-Windows: stub types so the file still compiles if you ever port it. */
  typedef int BOOL;
  #define TRUE 1
  #define FALSE 0
#endif

/* ------------------------------------------------------------------------- */
/* Money constraints                                                         */
/* ------------------------------------------------------------------------- */

#define MAX_PLAYER_MONEY  18446744073709551615ULL   /* Max for uint64.       */
#define MIN_PLAYER_MONEY  10ULL                     /* Minimum to play.      */

/* ------------------------------------------------------------------------- */
/* Globals                                                                   */
/* ------------------------------------------------------------------------- */


/* Background threads + control flags (Win32 only). */
#ifdef _WIN32
  static volatile BOOL gStopTimeThread      = FALSE;
  static volatile BOOL gStopAutosaveThread  = FALSE;
  static HANDLE        gTimeThreadHandle    = NULL;
  static HANDLE        gAutosaveThreadHandle= NULL;
#endif

/* ------------------------------------------------------------------------- */
/* Forward declarations (menus, helpers)                                     */
/* ------------------------------------------------------------------------- */

static void normalize_config(GameConfig *cfg);
static void normalize_player_counters(PlayerData *pd);

void deckMenu(void);
void gamesMenu(void);
void otherMenu(void);
void gameRules(void);
void customRules(void);
void jokers(void);
void numberOfDecks(void);
void ensureWinnableSolutions(void);
void autosave_menu(void);
void changeFunds(void);
void resetStatistics(void);
void resetAchievements(void);
void statsDisplay(void);
void printAchievements(void);

/* ------------------------------------------------------------------------- */
/* Threads (Win32)                                                           */
/* ------------------------------------------------------------------------- */

#ifdef _WIN32
/**
 * trackTimePlayedThread
 * Update time played counters (sec->min->hr) once per second.
 */
static DWORD WINAPI trackTimePlayedThread(LPVOID /*unused*/)
{
    while (!gStopTimeThread) {
        Sleep(1000);
        playerData.time_played_seconds++;
        if (playerData.time_played_seconds >= 60) {
            playerData.time_played_minutes++;
            playerData.time_played_seconds = 0;
        }
        if (playerData.time_played_minutes >= 60) {
            playerData.time_played_hours++;
            playerData.time_played_minutes = 0;
        }
    }
    return 0;
}

/**
 * autosave_worker
 * Every X minutes (config.autosave), write player+config to disk.
 * Sleeps in small steps so UI changes to the frequency are picked up quickly.
 */
static DWORD WINAPI autosave_worker(LPVOID /*unused*/)
{
    const int kMsPerMinute = 60000;
    const int kTickMs      = 1000;

    int elapsedMs = 0;
    while (!gStopAutosaveThread) {
        /* If autosave is disabled, idle lightly. */
        if (config.autosave <= 0) {
            Sleep(kTickMs);
            continue;
        }

        /* Run save when elapsed crosses threshold; else keep sleeping. */
        if (elapsedMs >= config.autosave * kMsPerMinute) {
            save_player_data();
            save_achievements();
            elapsedMs = 0;
        }
        Sleep(kTickMs);
        elapsedMs += kTickMs;
    }
    return 0;
}

static void start_time_thread(void)
{
    if (!gTimeThreadHandle) {
        gStopTimeThread   = FALSE;
        gTimeThreadHandle = CreateThread(NULL, 0, trackTimePlayedThread, NULL, 0, NULL);
        if (!gTimeThreadHandle) {
            printf("Failed to create time tracking thread.\n");
        }
    }
}

static void start_autosave_thread_if_needed(void)
{
    if (!gAutosaveThreadHandle) {
        gStopAutosaveThread   = FALSE;
        gAutosaveThreadHandle = CreateThread(NULL, 0, autosave_worker, NULL, 0, NULL);
        if (!gAutosaveThreadHandle) {
            printf("Failed to create autosave thread.\n");
        }
    }
}

static void stop_threads_and_cleanup(void)
{
    if (gTimeThreadHandle) {
        gStopTimeThread = TRUE;
        WaitForSingleObject(gTimeThreadHandle, INFINITE);
        CloseHandle(gTimeThreadHandle);
        gTimeThreadHandle = NULL;
    }
    if (gAutosaveThreadHandle) {
        gStopAutosaveThread = TRUE;
        WaitForSingleObject(gAutosaveThreadHandle, INFINITE);
        CloseHandle(gAutosaveThreadHandle);
        gAutosaveThreadHandle = NULL;
    }
}
#endif /* _WIN32 */

/* ------------------------------------------------------------------------- */
/* Small utilities                                                           */
/* ------------------------------------------------------------------------- */

/** Clamp and sanitize configuration values loaded from disk. */
static void normalize_config(GameConfig *cfg)
{
    if (!cfg) return;

    if (cfg->num_decks < 1) cfg->num_decks = 1;
    if (cfg->num_decks > 8) cfg->num_decks = 8;

    if (cfg->autosave < 0)  cfg->autosave  = 0;
    if (cfg->autosave > 60) cfg->autosave  = 60;

    /* booleans are fine as-is; leave jokers/DFS/backtracking untouched except range */
    cfg->depth_first_search = !!cfg->depth_first_search;
    cfg->backtracking       = !!cfg->backtracking;
    cfg->jokers             = !!cfg->jokers;
}

/** Fix inconsistent aggregate counters (defensive after load). */
static void normalize_player_counters(PlayerData *pd)
{
    if (!pd) return;

    const int bj_total    = pd->blackjack.wins + pd->blackjack.losses + pd->blackjack.draws;
    const int sol_total   = pd->solitaire.wins + pd->solitaire.losses + pd->solitaire.draws;
    const int idiot_total = pd->idiot.wins + pd->idiot.losses + pd->idiot.draws;

    pd->games_played = bj_total + sol_total + idiot_total;
    pd->total_wins   = pd->blackjack.wins + pd->solitaire.wins + pd->idiot.wins;
    pd->total_losses = pd->blackjack.losses + pd->solitaire.losses + pd->idiot.losses;
    pd->total_draws  = pd->blackjack.draws + pd->solitaire.draws + pd->idiot.draws;

    if (pd->uPlayerMoney > MAX_PLAYER_MONEY) {
        pd->uPlayerMoney = MAX_PLAYER_MONEY;
    }
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void)
{
    /* Seed PRNG for shuffles across the program. */
    srand((unsigned int)time(NULL));

    /* Ensure save directories exist before any I/O */
    fs_init();

    /* Defaults for a fresh run (will be overwritten by load if present). */
    globals_init();

    /* Start background time tracking immediately. */
#ifdef _WIN32
    start_time_thread();
#endif

    /* Initialize achievements registry (creates file on first run). */
    initialize_achievements();

    /* Load persisted profile (if exists) and normalize values. */
    if (load_player_data()) {
        normalize_config(&config);
        normalize_player_counters(&playerData);

#ifdef _WIN32
        /* If autosave was on last session, ensure worker thread exists. */
        if (!gAutosaveThreadHandle && config.autosave > 0) {
            start_autosave_thread_if_needed();
        }
#endif
        deckMenu();
        /* NOTREACHED in normal flow. */
        return 0;
    }

    /* First-run onboarding: ask for starting funds. */
    do {
        clear_screen();
        printf("Welcome to the Playing Card Simulation!\n");
        printf("Enter your starting money amount: $");
        scanf("%llu", &playerData.uPlayerMoney);

        if (playerData.uPlayerMoney < MIN_PLAYER_MONEY) {
            printf("You inputted $%llu\n", playerData.uPlayerMoney);
            printf("You need at least $%llu to play. ", MIN_PLAYER_MONEY);
            pause_for_enter();
        } else if (playerData.uPlayerMoney > MAX_PLAYER_MONEY) {
            printf("You cannot enter more than $%llu. ", MAX_PLAYER_MONEY);
            pause_for_enter();
        }
    } while (playerData.uPlayerMoney < MIN_PLAYER_MONEY ||
             playerData.uPlayerMoney > MAX_PLAYER_MONEY);

    playerData.starting_balance = playerData.uPlayerMoney;
    save_player_data();
    deckMenu();

#ifdef _WIN32
    stop_threads_and_cleanup();
#endif
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Main menu                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * deckMenu
 * Top-level menu for general actions and navigation into games/other screens.
 */
void deckMenu(void)
{
    Card singleDeck[DECK_SIZE];
    initialize_deck(singleDeck);

    while (1) {
        clear_screen();
        printf("=== MAIN MENU ===\n");
        printf("Funds: $%llu\n", playerData.uPlayerMoney);
        printf("\nPlease select from the options below:\n");
        printf("1: Initialize New Deck\n");
        printf("2: Shuffle Deck\n");
        printf("3: Print Deck\n");
        printf("4: Card Games Selection\n");
        printf("5: Change Funds\n");
        printf("6: Other\n");
        printf("7: Exit\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                printf("\nInitializing a new deck.\n");
                initialize_deck(singleDeck);
                pause_for_enter();
                break;

            case 2:
                printf("\nShuffling the deck.\n");
                shuffle_deck(singleDeck);
                pause_for_enter();
                break;

            case 3:
                printf("\nPrinting the deck.\n\n");
                print_deck(singleDeck);
                pause_for_enter();
                break;

            case 4:
                gamesMenu();
                break;

            case 5:
                changeFunds();
                break;

            case 6:
                otherMenu();
                break;

            case 7:
                printf("\nExiting.\n");
                save_player_data();
#ifdef _WIN32
                stop_threads_and_cleanup();
#endif
                exit(0);
                break;

            default:
                printf("\nPlease select a valid option (1-7)\n");
                pause_for_enter();
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Games menu                                                                */
/* ------------------------------------------------------------------------- */

void gamesMenu(void)
{
    while (1) {
        clear_screen();
        printf("=== GAME MENU ===\n");
        printf("Funds: $%llu\n", playerData.uPlayerMoney);
        printf("\nSelect a game:\n");
        printf("1: 21 Blackjack\n");
        printf("2: Texas Hold'em\n");
        printf("3: 5-Card Poker\n");
        printf("4: Solitaire\n");
        printf("5: Rummy\n");
        printf("6: Idiot\n");
        printf("7: Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                clear_screen();
                save_player_data();
                blackjack();        /* open Blackjack UI/menu */
                break;

            case 2:
                clear_screen();
                printf("=== Texas Hold'em ===\n");
                printf("\nGame is currently in development.\n");
                pause_for_enter();
                break;

            case 3:
                clear_screen();
                printf("=== 5-Card Poker ===\n");
                printf("\nGame is currently in development.\n");
                pause_for_enter();
                break;

            case 4:
                clear_screen();
                save_player_data();
                solitaire();
                break;

            case 5:
                clear_screen();
                printf("=== Rummy ===\n");
                printf("\nGame is currently in development.\n");
                pause_for_enter();
                break;

            case 6:
                clear_screen();
                save_player_data();
                idiot();
                break;

            case 7:
                deckMenu();             /* Back to main menu. */
                break;

            default:
                printf("\nPlease select a valid option (1-7)\n");
                pause_for_enter();
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Other menu                                                                */
/* ------------------------------------------------------------------------- */

void otherMenu(void)
{
    while (1) {
        clear_screen();
        printf("=== OTHER MENU ===\n");
        printf("Funds: $%llu\n", playerData.uPlayerMoney);
        printf("\nSelect an option:\n");
        printf("1: Change Game Rules\n");
        printf("2: View Achievements\n");
        printf("3: View Game Statistics\n");
        printf("4: Reset Statistics & Achievements\n");
        printf("5: Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                gameRules();
                break;

            case 2:
                clear_screen();
                printAchievements();
                pause_for_enter();
                break;

            case 3:
                clear_screen();
                statsDisplay();
                pause_for_enter();
                break;

            case 4:
                clear_screen();
                resetStatistics();
                break;

            case 5:
                return;

            default:
                printf("\nPlease select a valid option (1-5)\n");
                pause_for_enter();
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Rules + customizations                                                    */
/* ------------------------------------------------------------------------- */

void gameRules(void)
{
    while (1) {
        clear_screen();
        printf("=== GAME RULES ===\n");
        printf("Funds: $%llu\n", playerData.uPlayerMoney);
        printf("\nSelect a category:\n");
        printf("1: Custom Rules\n");
        printf("2. Autosave\n");
        printf("3: Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                customRules();
                break;
            case 2:
                autosave_menu();
                break;
            case 3:
                return;
            default:
                printf("\nPlease select a valid option (1-3)\n");
                pause_for_enter();
                break;
        }
    }
}

void customRules(void)
{
    while (1) {
        clear_screen();
        printf("=== CUSTOM GAME RULES ===\n");
        printf("Funds: $%llu\n", playerData.uPlayerMoney);
        printf("\nSelect a rule to change:\n");
        printf("1: Jokers\n");
        printf("2. Number of Decks\n");
        printf("3: Ensure Winnable Solutions (Solitaire)\n");
        printf("4: Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                jokers();
                break;
            case 2:
                numberOfDecks();
                break;
            case 3:
                ensureWinnableSolutions();
                break;
            case 4:
                return;
            default:
                printf("\nPlease select a valid option (1-4)\n");
                pause_for_enter();
                break;
        }
    }
}

void jokers(void)
{
    while (1) {
        clear_screen();
        printf("=== JOKERS ===\n");
        printf("%s\n\n", config.jokers ? "Enabled" : "Disabled");

        printf("Jokers can be used in some games (e.g., Idiot) as a wild card.\n");
        printf("When enabled, two jokers are shuffled into the deck.\n\n");

        printf("1: Toggle Jokers (%s)\n", config.jokers ? "Disable" : "Enable");
        printf("2. Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                config.jokers = !config.jokers;
                save_player_data();
                break;
            case 2:
                return;
            default:
                printf("\nPlease select a valid option (1-2)\n");
                pause_for_enter();
                break;
        }
    }
}

void numberOfDecks(void)
{
    const int minDecks = 1;
    const int maxDecks = 8;

    while (1) {
        clear_screen();
        printf("=== NUMBER OF DECKS ===\n");
        printf("Current number of decks: %d\n\n", config.num_decks);

        printf("Blackjack can use up to %d decks (shoe).\n", maxDecks);
        printf("Solitaire is always 1 deck.\n\n");
        printf("1: Change Deck Count\n");
        printf("2. Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1: {
                int newCount = config.num_decks;
                while (1) {
                    printf("Enter new number of decks (%d-%d)\n> ", minDecks, maxDecks);
                    scanf("%d", &newCount);
                    if (newCount >= minDecks && newCount <= maxDecks) break;
                    printf("\nPlease select a valid number of decks (%d-%d)\n", minDecks, maxDecks);
                }
                config.num_decks = newCount;
                save_player_data();
                break;
            }
            case 2:
                return;
            default:
                printf("\nPlease select a valid option (1-2)\n");
                pause_for_enter();
                break;
        }
    }
}

void ensureWinnableSolutions(void)
{
    while (1) {
        clear_screen();
        printf("=== ENSURE WINNABLE SOLUTIONS (Solitaire) ===\n");
        if (!config.depth_first_search && !config.backtracking) {
            printf("Mode: Disabled\n\n");
        } else if (config.depth_first_search) {
            printf("Mode: Depth First Search\n\n");
        } else if (config.backtracking) {
            printf("Mode: Backtracking\n\n");
        }

        printf("This setting tries to ensure Solitaire deals are winnable.\n");
        printf("Depth First Search: explore move tree until solution or exhaustion.\n");
        printf("Backtracking: (placeholder) incremental construction (not implemented).\n\n");

        printf("1: Toggle Depth First Search (%s)\n", config.depth_first_search ? "Disable" : "Enable");
        printf("2. Toggle Backtracking         (%s)\n", config.backtracking       ? "Disable" : "Enable");
        printf("3: Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1:
                config.depth_first_search = !config.depth_first_search;
                if (config.depth_first_search) config.backtracking = false;
                save_player_data();
                break;

            case 2:
                config.backtracking = !config.backtracking;
                if (config.backtracking) config.depth_first_search = false;
                save_player_data();
                break;

            case 3:
                return;

            default:
                printf("\nPlease select a valid option (1-3)\n");
                pause_for_enter();
                break;
        }
    }
}

void autosave_menu(void)
{
    const int minAutosave = 0;
    const int maxAutosave = 60;

    while (1) {
        clear_screen();
        printf("=== AUTOSAVE ===\n");
        if (config.autosave == 0) printf("Current: OFF\n\n");
        else                      printf("Current: %d minute(s)\n\n", config.autosave);

        printf("Autosave writes your profile to disk periodically (0 disables).\n");
        printf("Max interval is %d minutes.\n\n", maxAutosave);

        printf("1: Change autosave frequency\n");
        printf("2. Back\n");
        printf("> ");

        int menuSelectionOption = 0;
        scanf("%d", &menuSelectionOption);

        switch (menuSelectionOption) {
            case 1: {
                int newFreq = config.autosave;
                while (1) {
                    printf("Enter new autosave frequency (%d-%d minutes)\n> ", minAutosave, maxAutosave);
                    scanf("%d", &newFreq);
                    if (newFreq >= minAutosave && newFreq <= maxAutosave) break;
                    printf("\nPlease select a valid autosave frequency (%d-%d minutes)\n", minAutosave, maxAutosave);
                }
                config.autosave = newFreq;
                save_player_data();
#ifdef _WIN32
                /* Ensure worker exists; it handles 0 internally by idling. */
                start_autosave_thread_if_needed();
#endif
                break;
            }
            case 2:
                return;

            default:
                printf("\nPlease select a valid option (1-2)\n");
                pause_for_enter();
                break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Money + stats + achievements                                              */
/* ------------------------------------------------------------------------- */

void changeFunds(void)
{
    do {
        printf("\nPlease enter the desired funds: $");
        scanf("%llu", &playerData.uPlayerMoney);

        if (playerData.uPlayerMoney < MIN_PLAYER_MONEY) {
            printf("You need at least $%llu to play. ", MIN_PLAYER_MONEY);
            pause_for_enter();
        } else if (playerData.uPlayerMoney > MAX_PLAYER_MONEY) {
            printf("You cannot enter more than $%llu. ", MAX_PLAYER_MONEY);
            pause_for_enter();
        }
    } while (playerData.uPlayerMoney < MIN_PLAYER_MONEY ||
             playerData.uPlayerMoney > MAX_PLAYER_MONEY);

    playerData.starting_balance = playerData.uPlayerMoney;
    printf("\n");
    save_player_data();
}

void resetStatistics(void)
{
    printf("Are you sure you want to reset player data and achievements?\n");
    printf("This action cannot be undone.\n");
    printf("1: Yes\n");
    printf("2. No\n");
    printf("> ");

    int userChoice = 0;
    scanf("%d", &userChoice);

    switch (userChoice) {
        case 1:
            memset(&playerData, 0, sizeof(PlayerData));
            resetAchievements();
            changeFunds();         /* also re-sets starting_balance */
            save_player_data();
            save_achievements();
            break;
        case 2:
            break;
        default:
            printf("Please input a valid option (1-2)\n");
            pause_for_enter();
            break;
    }
}

void resetAchievements(void)
{
    /* Mark all as locked; the achievements module owns the array. */
    for (int i = 0; i < achievement_count; ++i) {
        achievements[i].unlocked = false;
    }
}

void statsDisplay(void)
{
    printf("=== Statistics ===\n\n");

    if (playerData.starting_balance <= playerData.uPlayerMoney) {
        printf("Profit: $%llu\n", (playerData.uPlayerMoney - playerData.starting_balance));
    } else {
        printf("Loss:   $%llu\n", (playerData.starting_balance - playerData.uPlayerMoney));
    }

    printf("Time Played: %d:%02d:%02d\n",
           playerData.time_played_hours,
           playerData.time_played_minutes,
           playerData.time_played_seconds);

    printf("\n21 Blackjack\n");
    printf("Wins: %d\n",  playerData.blackjack.wins);
    printf("Losses: %d\n",playerData.blackjack.losses);
    printf("Draws: %d\n", playerData.blackjack.draws);
    printf("Win Streak: %d\n", playerData.blackjack.max_win_streak);

    printf("\nSolitaire\n");
    printf("Wins: %d\n",  playerData.solitaire.wins);
    printf("Losses: %d\n",playerData.solitaire.losses);
    printf("Draws: %d\n", playerData.solitaire.draws);
    printf("Win Streak: %d\n", playerData.solitaire.max_win_streak);

    printf("\nIdiot\n");
    printf("Wins: %d\n",  playerData.idiot.wins);
    printf("Losses: %d\n",playerData.idiot.losses);
    printf("Draws: %d\n", playerData.idiot.draws);
    printf("Win Streak: %d\n", playerData.idiot.max_win_streak);

    save_player_data();
}

void printAchievements(void)
{
    /* Sync any newly satisfied criteria. */
    checkAchievements();

    /* Tally unlocked. */
    int unlockedCount = 0;
    for (int i = 0; i < achievement_count; ++i) {
        if (achievements[i].unlocked) unlockedCount++;
    }

    printf("=== Achievements ===\n");
    printf("%d/%d Unlocked\n", unlockedCount, achievement_count);

    const int general_start   = 0;
    const int blackjack_start = MAX_GENERAL_ACHIEVEMENTS;
    const int solitaire_start = blackjack_start + MAX_BLACKJACK_ACHIEVEMENTS;
    const int idiot_start     = solitaire_start + MAX_SOLITAIRE_ACHIEVEMENTS;
    const int hidden_start    = idiot_start + MAX_IDIOT_ACHIEVEMENTS;

    printAchievementCategory("General Achievements",   general_start,   MAX_GENERAL_ACHIEVEMENTS);
    printAchievementCategory("Blackjack Achievements", blackjack_start, MAX_BLACKJACK_ACHIEVEMENTS);
    printAchievementCategory("Solitaire Achievements", solitaire_start, MAX_SOLITAIRE_ACHIEVEMENTS);
    printAchievementCategory("Idiot Achievements",     idiot_start,     MAX_IDIOT_ACHIEVEMENTS);

    /* Sorted print for hidden achievements that are unlocked. */
    Achievement *unlocked_hidden[MAX_HIDDEN_ACHIEVEMENTS];
    int hiddenUnlockedCount = 0;

    for (int i = hidden_start;
         i < hidden_start + MAX_HIDDEN_ACHIEVEMENTS && i < achievement_count;
         ++i) {
        if (achievements[i].unlocked) {
            unlocked_hidden[hiddenUnlockedCount++] = &achievements[i];
        }
    }

    if (hiddenUnlockedCount > 0) {
        qsort(unlocked_hidden, hiddenUnlockedCount, sizeof(Achievement*), achievement_name_cmp);
        printf("\nHidden Achievements\n");
        for (int i = 0; i < hiddenUnlockedCount; ++i) {
            printf("[X] %s: %s\n", unlocked_hidden[i]->name, unlocked_hidden[i]->description);
        }
    }
}


/**
 * read_menu_choice
 * Robustly read an integer menu selection in [minOption..maxOption].
 * Discards non-numeric input and reprompts until valid.
 */
static int read_menu_choice(int minOption, int maxOption) {
    int choice, scanned;
    for (;;) {
        printf("> ");
        scanned = scanf("%d", &choice);
        if (scanned == 1 && choice >= minOption && choice <= maxOption) {
            return choice;
        }
        // Flush bad input and reprompt
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { /* discard */ }
        printf("Please select a valid option (%d-%d)\n", minOption, maxOption);
    }
}

/* ------------------------------- */
/* PUBLIC  GAME MENUS              */
/* ------------------------------- */

void blackjack(void) {
    int menuSelection;
    for (;;) {
        clear_screen();
        printf("=== 21 BLACKJACK ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Back\n");

        menuSelection = read_menu_choice(1, 3);
        switch (menuSelection) {
            case 1:
                blackjack_start();
                break;
            case 2:
                blackjackHowToPlay();
                break;
            case 3:
                gamesMenu();
                break;
        }
    }
}

void solitaire(void) {
    int menuSelection;
    for (;;) {
        clear_screen();
        printf("=== SOLITAIRE ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Back\n");

        menuSelection = read_menu_choice(1, 3);
        switch (menuSelection) {
            case 1:
                solitaire_start();
                break;
            case 2:
                solitaireHowToPlay();
                break;
            case 3:
                gamesMenu();
                break;
        }
    }
}

void idiot(void) {
    int menuSelection;
    for (;;) {
        clear_screen();
        printf("=== IDIOT ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Back\n");

        menuSelection = read_menu_choice(1, 3);
        switch (menuSelection) {
            case 1:
                idiot_start();
                break;
            case 2:
                idiotHowToPlay();
                break;
            case 3:
                gamesMenu();
                break;
        }
    }
}
