/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * Achievement initialization, persistence, and criteria
 *
 * This module:
 *  - registers all known achievements in well-defined category order,
 *  - offers save/load to a compact binary file,
 *  - exposes helpers to unlock and render achievements,
 *  - defines criteria predicates read from global playerData stats.
 */

#include "achievements.h"
#include "paths.h"

Achievement achievements[MAX_ACHIEVEMENTS];

/* ------------------------------------------------------------------------- */
/* File-scope data                                                           */
/* ------------------------------------------------------------------------- */

int achievement_count = 0;

/* Table of (name, predicate) pairs used by checkAchievements() */
AchievementCheck achievementChecks[] = {
    /* General */
    { "First Shuffle",        firstShuffleCriteria       },
    { "Persistent Player",    persistentPlayerCriteria   },
    { "Card Novice",          cardNoviceCriteria         },
    { "Card Apprentice",      cardApprenticeCriteria     },
    { "Card Profit",          cardProfitCriteria         },
    { "Card Master",          cardMasterCriteria         },

    /* 21 Blackjack */
    { "21 Blackjack",         blackjackWinCriteria       },
    { "Double Trouble",       doubleTroubleCriteria      },
    { "Insurance Payout",     insurancePayoutCriteria    },
    { "Risk Taker",           riskTakerCriteria          },
    { "Lucky Streak",         luckyStreakCriteria        },

    /* Solitaire */
    { "Perfect Clear",        perfectClearCriteria       },
    { "Solitaire Novice",     solitaireNoviceCriteria    },
    { "Solitaire Apprentice", solitaireApprenticeCriteria},
    { "Solitaire Master",     solitaireMasterCriteria    },
    { "The Long Game",        theLongGameCriteria        },

    /* Idiot */
    { "Not the Idiot",        notTheIdiotCriteria        },
    { "Mirror Match",         mirrorMatchCriteria        },
    { "Pyrotechnic",          pyrotechnicCriteria        },
    { "4 of a Kind",          fourOfAKindCriteria        },
    { "The Trickster",        theTricksterCriteria       },

    /* Hidden */
    { "Time Master",          timeMasterCriteria         },
    { "Infinite Wealth",      infiniteWealthCriteria     },
    { "From Rags to Riches",  fromRagsToRichesCriteria   },
    { "The Collector",        theCollectorCriteria       },
};

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

/* Safe bounded copy that always NUL-terminates the destination. */
static void copy_str_safe(char *dst, size_t dst_cap, const char *src)
{
    if (!dst || !dst_cap) { return; }
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Initialization                                                            */
/* ------------------------------------------------------------------------- */

void initialize_achievements(void)
{
    achievement_count = 0;
    memset(achievements, 0, sizeof(achievements));

    /* General */
    add_achievement("First Shuffle",       "Play your first game.");
    add_achievement("Persistent Player",   "Play 100 games.");
    add_achievement("Card Novice",         "Win 5 games.");
    add_achievement("Card Apprentice",     "Win 10 games.");
    add_achievement("Card Profit",         "Win 25 games.");
    add_achievement("Card Master",         "Win 50 games.");

    /* 21 Blackjack */
    add_achievement("21 Blackjack",        "Win with a blackjack.");
    add_achievement("Double Trouble",      "Win or Draw after doubling down.");
    add_achievement("Insurance Payout",    "Successfully use insurance.");
    add_achievement("Risk Taker",          "Win both hands after splitting.");
    add_achievement("Lucky Streak",        "Win 10 games in a row.");

    /* Solitaire */
    add_achievement("Perfect Clear",       "Win without using an undo.");
    add_achievement("Solitaire Novice",    "Win 1 game on easy difficulty.");
    add_achievement("Solitaire Apprentice","Win 1 game on normal difficulty.");
    add_achievement("Solitaire Master",    "Win 1 game on hard difficulty.");
    add_achievement("The Long Game",       "Take at least 30 minutes to complete a game.");

    /* Idiot */
    add_achievement("Not the Idiot",       "Win 1 game of Idiot.");
    add_achievement("Mirror Match",        "Play a 3 against another 3.");
    add_achievement("Pyrotechnic",         "Burn the pile 10 times.");
    add_achievement("4 of a Kind",         "Burn the pile with 4 of a kind.");
    add_achievement("The Trickster",       "Win a game without picking up the pile.");

    /* Hidden */
    add_achievement("Time Master",         "Play for at least 1 hour.");
    add_achievement("Infinite Wealth",     "Have a starting balance of at least $1,000,000.");
    add_achievement("From Rags to Riches", "Start with a balance less than $100 and earn at least $10,000.");
    add_achievement("The Collector",       "Collect all achievements.");

    /* Load persistent state; if none exists yet, create it. */
    if (load_achievements() != 0) {
        (void)save_achievements();
    }
}

/* ------------------------------------------------------------------------- */
/* CRUD / management                                                         */
/* ------------------------------------------------------------------------- */

int add_achievement(const char *name, const char *description)
{
    if (achievement_count >= MAX_ACHIEVEMENTS) { return -1; }

    copy_str_safe(achievements[achievement_count].name,
                  sizeof(achievements[achievement_count].name),
                  name);

    copy_str_safe(achievements[achievement_count].description,
                  sizeof(achievements[achievement_count].description),
                  description);

    achievements[achievement_count].unlocked        = false;
    achievements[achievement_count].hidden_unlocked = false;

    ++achievement_count;
    return 0;
}

int unlock_achievement(const char *name)
{
    for (int i = 0; i < achievement_count; ++i)
    {
        if (strcmp(achievements[i].name, name) == 0 && !achievements[i].unlocked)
        {
            achievements[i].unlocked = true;
            printf("Achievement unlocked: %s\n", achievements[i].name);
            return 1; /* success */
        }
    }
    return 0; /* not found or already unlocked */
}

int is_achievement_unlocked(const char *name)
{
    for (int i = 0; i < achievement_count; ++i)
    {
        if (strcmp(achievements[i].name, name) == 0)
        {
            return achievements[i].unlocked ? 1 : 0;
        }
    }
    return 0;
}

void list_achievements(void)
{
    printf("Achievements:\n");
    for (int i = 0; i < achievement_count; ++i)
    {
        printf("[%c] %s: %s\n",
               achievements[i].unlocked ? 'X' : ' ',
               achievements[i].name,
               achievements[i].description);
    }
}

/* ------------------------------------------------------------------------- */
/* Persistence                                                               */
/* ------------------------------------------------------------------------- */

int save_achievements(void)
{
    FILE *file = fopen(ACHIEVEMENTS_PATH, "wb");
    if (!file) { return -1; }  /* could not open file */

    if (fwrite(&achievement_count, sizeof(int), 1, file) != 1)
    {
        fclose(file);
        return -2;  /* Error writing count */
    }

    if (fwrite(achievements, sizeof(Achievement), (size_t)achievement_count, file)
        != (size_t)achievement_count)
    {
        fclose(file);
        return -3;  /* Error writing array */
    }

    fclose(file);
    return 0;
}

int load_achievements(void)
{
    FILE *file = fopen(ACHIEVEMENTS_PATH, "rb");
    if (!file) { return -1; }  /* could not open file */

    if (fread(&achievement_count, sizeof(int), 1, file) != 1)
    {
        fclose(file);
        return -2;  /* Error reading count */
    }

    if (achievement_count < 0)                   { achievement_count = 0; }
    if (achievement_count > MAX_ACHIEVEMENTS)    { achievement_count = MAX_ACHIEVEMENTS; }

    if (fread(achievements, sizeof(Achievement), (size_t)achievement_count, file)
        != (size_t)achievement_count)
    {
        fclose(file);
        return -3;  /* Error reading array */
    }

    fclose(file);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Presentation helpers                                                      */
/* ------------------------------------------------------------------------- */

void printAchievementCategory(const char *categoryName, int start, int count)
{
    if (!categoryName || count <= 0 || start >= achievement_count) { return; }
    if (start < 0) { start = 0; }

    int end = start + count;
    if (end > achievement_count) { end = achievement_count; }

    printf("\n%s\n", categoryName);

    for (int i = start; i < end; ++i)
    {
        printf("[%c] %s: %s\n",
               achievements[i].unlocked ? 'X' : ' ',
               achievements[i].name,
               achievements[i].description);
    }
}

/* ------------------------------------------------------------------------- */
/* Criteria evaluation                                                       */
/* ------------------------------------------------------------------------- */

void checkAchievements(void)
{
    const size_t n = sizeof(achievementChecks) / sizeof(achievementChecks[0]);

    for (size_t i = 0; i < n; ++i)
    {
        if (achievementChecks[i].criteria_func() &&
            !is_achievement_unlocked(achievementChecks[i].name))
        {
            (void)unlock_achievement(achievementChecks[i].name);
        }
    }
}

/* Comparator for qsort of Achievement* arrays by name ascending. */
int achievement_name_cmp(const void *a, const void *b)
{
    const Achievement *achA = *(const Achievement *const *)a;
    const Achievement *achB = *(const Achievement *const *)b;
    return strcmp(achA->name, achB->name);
}

/* Debug function to unlock all achievements and persist to disk. */
void unlockAllAchievements(void)
{
    for (int i = 0; i < achievement_count; ++i)
    {
        achievements[i].unlocked = true;
    }
    (void)save_achievements();
}

/* ------------------------------------------------------------------------- */
/* Criteria predicates                                                       */
/* ------------------------------------------------------------------------- */

/* General */
bool firstShuffleCriteria(void)      { return playerData.games_played           >= 1; }
bool persistentPlayerCriteria(void)  { return playerData.games_played           >= 100; }
bool cardNoviceCriteria(void)        { return playerData.total_wins             >= 5; }
bool cardApprenticeCriteria(void)    { return playerData.total_wins             >= 10; }
bool cardProfitCriteria(void)        { return playerData.total_wins             >= 25; }
bool cardMasterCriteria(void)        { return playerData.total_wins             >= 50; }

/* 21 Blackjack */
bool blackjackWinCriteria(void)      { return playerData.blackjack.blackjack_wins   >= 1; }
bool doubleTroubleCriteria(void)     { return playerData.blackjack.doubledown_wins  >= 1; }
bool insurancePayoutCriteria(void)   { return playerData.blackjack.insurance_success>= 1; }
bool riskTakerCriteria(void)         { return playerData.blackjack.split_wins       >= 1; }
bool luckyStreakCriteria(void)       { return playerData.blackjack.max_win_streak   >= 10; }

/* Solitaire */
bool perfectClearCriteria(void)      { return playerData.solitaire.perfect_clear        >= 1; }
bool solitaireNoviceCriteria(void)   { return playerData.solitaire.easy_wins            >= 1; }
bool solitaireApprenticeCriteria(void){ return playerData.solitaire.normal_wins         >= 1; }
bool solitaireMasterCriteria(void)   { return playerData.solitaire.hard_wins            >= 1; }
bool theLongGameCriteria(void)       { return playerData.solitaire.longest_game_minutes >= 30; }

/* Idiot */
bool notTheIdiotCriteria(void)       { return playerData.idiot.wins                 >= 1; }
bool mirrorMatchCriteria(void)       { return playerData.idiot.mirror_match         >= 1; }
bool pyrotechnicCriteria(void)       { return playerData.idiot.burns                >= 10; }
bool fourOfAKindCriteria(void)       { return playerData.idiot.four_of_a_kind_burns >= 1; }
bool theTricksterCriteria(void)      { return playerData.idiot.trickster_wins       >= 1; }

/* Hidden */
bool timeMasterCriteria(void)        { return playerData.time_played_hours >= 1; }
bool infiniteWealthCriteria(void)    { return playerData.starting_balance >= 1000000; }
bool fromRagsToRichesCriteria(void)  { return (playerData.starting_balance < 100) && (playerData.uPlayerMoney >= 10000); }
bool theCollectorCriteria(void)
{
    /* All but the very last ("The Collector") must be unlocked. */
    for (int i = 0; i < achievement_count - 1; ++i)
    {
        if (!achievements[i].unlocked) { return false; }
    }
    return true;
}