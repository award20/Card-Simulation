/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * Achievement model, categories, and public API
 *
 * Exposes the Achievement type, constants, criteria function prototypes,
 * and management routines (init, save/load, unlock, listing).
 */

#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include "core.h"

/* ------------------------------------------------------------------------- */
/* Limits and storage                                                        */
/* ------------------------------------------------------------------------- */

#define MAX_ACHIEVEMENTS                 25

#define MAX_GENERAL_ACHIEVEMENTS          6
#define MAX_BLACKJACK_ACHIEVEMENTS        5
#define MAX_SOLITAIRE_ACHIEVEMENTS        5
#define MAX_IDIOT_ACHIEVEMENTS            5
#define MAX_HIDDEN_ACHIEVEMENTS           4

#define MAX_ACHIEVEMENT_NAME_LENGTH      64
#define MAX_ACHIEVEMENT_DESCRIPTION_LENGTH 128

#define ACHIEVEMENT_SAVE                  "achievements/achievements.dat"

/* ------------------------------------------------------------------------- */
/* Data types                                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
    char name[MAX_ACHIEVEMENT_NAME_LENGTH];
    char description[MAX_ACHIEVEMENT_DESCRIPTION_LENGTH];
    bool unlocked;
    bool hidden_unlocked;
} Achievement;

/* Each entry pairs a display name with a boolean predicate to unlock it. */
typedef struct {
    const char *name;
    bool (*criteria_func)(void);
} AchievementCheck;

/* ------------------------------------------------------------------------- */
/* External state                                                            */
/* ------------------------------------------------------------------------- */

extern Achievement achievements[MAX_ACHIEVEMENTS];
extern AchievementCheck achievementChecks[];
extern int achievement_count;

/* ------------------------------------------------------------------------- */
/* Criteria prototypes                                                       */
/* ------------------------------------------------------------------------- */

/* General */
bool firstShuffleCriteria(void);
bool persistentPlayerCriteria(void);
bool cardNoviceCriteria(void);
bool cardApprenticeCriteria(void);
bool cardProfitCriteria(void);
bool cardMasterCriteria(void);

/* 21 Blackjack */
bool blackjackWinCriteria(void);
bool doubleTroubleCriteria(void);
bool insurancePayoutCriteria(void);
bool riskTakerCriteria(void);
bool luckyStreakCriteria(void);

/* Solitaire */
bool perfectClearCriteria(void);
bool solitaireNoviceCriteria(void);
bool solitaireApprenticeCriteria(void);
bool solitaireMasterCriteria(void);
bool theLongGameCriteria(void);

/* Idiot */
bool notTheIdiotCriteria(void);
bool mirrorMatchCriteria(void);
bool pyrotechnicCriteria(void);
bool fourOfAKindCriteria(void);
bool theTricksterCriteria(void);

/* Hidden */
bool timeMasterCriteria(void);
bool infiniteWealthCriteria(void);
bool fromRagsToRichesCriteria(void);
bool theCollectorCriteria(void);

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void initialize_achievements(void);
int  add_achievement(const char *name, const char *description);
int  unlock_achievement(const char *name);
int  is_achievement_unlocked(const char *name);
void list_achievements(void);
int  save_achievements(void);
int  load_achievements(void);

/* Utility render helpers */
void printAchievementCategory(const char *categoryName, int start, int count);

/* Evaluate and unlock newly met criteria */
void checkAchievements(void);

/* qsort comparator for arrays of Achievement* (by name asc). */
int  achievement_name_cmp(const void *a, const void *b);

/* Debug helper: unlocks all known achievements and saves. */
void unlockAllAchievements(void);

#endif /* ACHIEVEMENTS_H */