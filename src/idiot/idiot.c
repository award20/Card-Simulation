/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * Idiot card game implementation
 *
 * This file contains the full game logic for Idiot, including:
 *   - dealing & setup, optional Jokers,
 *   - rules (play legality, mirrors, burns),
 *   - the interactive loop (player input & UI),
 *   - Easy/Normal/Hard AI (Hard includes a greedy look-ahead scorer),
 *   - payouts/stat tracking on end of game.
 *
 * External dependencies (provided by the project):
 *   - deck.h: Card, DECK_SIZE, initialize_deck(), shuffle_deck()
 *   - clear_screen(), playerData, config (for jokers), checkAchievements(),
 *     save_player_data(), save_achievements()
 */

#include "idiot.h"

/* --------------------------------------------------------------------------- */
/* INTERNAL / FILE-LOCAL DECLARATIONS                                          */
/* --------------------------------------------------------------------------- */

/* ----- Small UI helpers ----- */
static void print_card_bracketed       (const Card *card);         /* Prints "[Joker]" or "[Rank of Suit]". */
static void print_hidden_brackets      (int count);                 /* Prints "[???] " count times.          */

/* ----- Core rule helpers ----- */
static int  card_value                  (const Card *card);         /* Map ranks to values; Joker==3.        */
static int  is_special_card             (const Card *card, int v);  /* True if the card’s value equals v.    */
static int  is_power_card               (const Card *card);         /* True for Joker, 2, 3, 10.             */
static int  can_play_card               (const Card *top, const Card *next, const CardPile *wastePile);
static int  is_four_of_a_kind           (CardPile *wastePile);
static void burn_pile                   (CardPile *wastePile);
static void draw_from_pile              (IdiotPlayer *playerState, CardPile *drawPile);
static void sort_hand_low_to_high       (IdiotPlayer *playerState);
static void handle_pile_pickup          (IdiotPlayer *playerState, CardPile *wastePile);
static Card* find_mirrored_card         (CardPile *wastePile);

/* ----- Setup & UI ----- */
static void swap_hand_cards             (IdiotPlayer *playerState); /* Optional face-up/hand swaps at start. */
static void display_idiot_game          (IdiotPlayer *playerState,
                                         IdiotPlayer *opponentState,
                                         CardPile    *drawPile,
                                         CardPile    *wastePile,
                                         AILastMove  *aiLastTurnSummary);

/* ----- AI utilities ----- */
static void          lm_reset           (AILastMove *summary);
static void          lm_record          (AILastMove *summary, Card played);
static int           duplicates_in_hand (const IdiotPlayer *playerState, const char *rankStr);
static int           top_run_length_by_value(const CardPile *pile);
static int           should_burn_now_with_10(const CardPile *pile, int difficulty);
static int           count_playable_for_next(const CardPile *pile, const IdiotPlayer *nextPlayer);
static int           hard_best_followup_index(const IdiotPlayer *aiState, const CardPile *pileAfterTwo);
static int           hard_score_candidate(const IdiotPlayer *aiState,
                                          const IdiotPlayer *opponentState,
                                          const CardPile    *pileState,
                                          const CardPile    *drawPileState,
                                          int fromHandZone,
                                          int indexInZone);
static Card          take_at            (Card *arr, int *countRef, int index); /* Remove at index, shift left. */
static int           dump_same_rank_in_hand(IdiotPlayer *aiState, CardPile *pile,
                                            const char *rankStr, int capToPlay,
                                            AILastMove *summary);

/* ----- AI driver ----- */
static void ai_play(IdiotPlayer *aiState,
                    IdiotPlayer *opponentState,
                    CardPile    *wastePile,
                    CardPile    *drawPile,
                    int          difficulty,
                    AILastMove  *aiLastTurnSummary);

/* ========================================================================== */
/* SMALL UI HELPERS                                                            */
/* ========================================================================== */

/**
 * print_card_bracketed
 * Render a single card in “[...]” form to stdout.
 */
static void print_card_bracketed(const Card *card) {
    if (card->is_joker) {
        printf("[Joker]");
    } else {
        printf("[%s of %s]", card->rank, card->suit);
    }
}

/**
 * print_hidden_brackets
 * Print “[???] ” repeatedly (used for concealed cards or opponent’s hand).
 */
static void print_hidden_brackets(int count) {
    for (int i = 0; i < count; ++i) printf("[???] ");
}

/* --------------------------------------------------------------------------- */
/* CORE RULE HELPERS                                                           */
/* --------------------------------------------------------------------------- */

/**
 * card_value
 * Map rank strings to integer values. Jokers function exactly like a “3” in
 * Idiot (wild mirror and always playable), so we return 3 for Jokers.
 *
 * Value order: Ace=14, King=13, Queen=12, Jack=11, 10..2 as integers.
 */
static int card_value(const Card *card) {
    if (card->is_joker)                     return 3;   /* Joker ≡ 3 */
    if (strcmp(card->rank, "Ace")   == 0)   return 14;
    if (strcmp(card->rank, "King")  == 0)   return 13;
    if (strcmp(card->rank, "Queen") == 0)   return 12;
    if (strcmp(card->rank, "Jack")  == 0)   return 11;
    return atoi(card->rank); /* "10".."2" */
}

/** True if the card’s value equals v. */
static int is_special_card(const Card *card, int v) {
    return card_value(card) == v;
}

/** True for Joker, 2, 3, 10 (the “power” cards). */
static int is_power_card(const Card *card) {
    return card->is_joker ||
           is_special_card(card, 2) ||
           is_special_card(card, 3) ||
           is_special_card(card, 10);
}

/**
 * can_play_card
 * Enforce Idiot’s placement rules:
 *  - If next is Joker / 2 / 3 / 10 → always playable.
 *  - If top is a 3 (or Joker), recursively “mirror” to the last non-3/Joker
 *    below the run and compare against that card’s value.
 *  - Otherwise, next must be >= top by value.
 */
static int can_play_card(const Card *top, const Card *next, const CardPile *wastePile) {
    /* Power cards are always playable. */
    if (next->is_joker || is_special_card(next, 2) || is_special_card(next, 3) || is_special_card(next, 10))
        return 1;

    if (!top) return 1;  /* Empty pile: anything goes. */

    /* Mirror effect: walk down over any chain of 3s/Jokers. */
    if (top->is_joker || is_special_card(top, 3)) {
        if (wastePile && wastePile->count > 1) {
            int idx = wastePile->count - 2;
            while (idx >= 0 &&
                   (wastePile->pile[idx].is_joker || is_special_card(&wastePile->pile[idx], 3))) {
                --idx;
            }
            return (idx >= 0) ? can_play_card(&wastePile->pile[idx], next, wastePile) : 1;
        }
        return 1;
    }

    /* Normal restriction: must be >= the top value. */
    return card_value(next) >= card_value(top);
}

/** Four of a kind on the pile burns it immediately. */
static int is_four_of_a_kind(CardPile *wastePile) {
    if (wastePile->count < 4) return 0;
    int topValue = card_value(&wastePile->pile[wastePile->count - 1]);
    for (int i = 2; i <= 4; ++i) {
        if (card_value(&wastePile->pile[wastePile->count - i]) != topValue)
            return 0;
    }
    return 1;
}

/** Burn: discard the waste pile entirely. */
static void burn_pile(CardPile *wastePile) {
    wastePile->count = 0;
}

/**
 * draw_from_pile
 * Draw until the hand reaches HAND_SIZE or the draw pile is empty.
 */
static void draw_from_pile(IdiotPlayer *playerState, CardPile *drawPile) {
    while (playerState->handCount < HAND_SIZE && drawPile->count > 0) {
        playerState->hand[playerState->handCount++] = drawPile->pile[--drawPile->count];
    }
}

/**
 * sort_hand_low_to_high
 * Simple O(n^2) sort by card value to present hand in ascending order.
 */
static void sort_hand_low_to_high(IdiotPlayer *playerState) {
    for (int i = 0; i < playerState->handCount - 1; ++i) {
        for (int j = i + 1; j < playerState->handCount; ++j) {
            if (card_value(&playerState->hand[i]) > card_value(&playerState->hand[j])) {
                Card tmp               = playerState->hand[i];
                playerState->hand[i]   = playerState->hand[j];
                playerState->hand[j]   = tmp;
            }
        }
    }
}

/**
 * handle_pile_pickup
 * Move the entire waste pile into the player’s hand (order preserved), then
 * clear the waste pile and resort the hand for readability.
 */
static void handle_pile_pickup(IdiotPlayer *playerState, CardPile *wastePile) {
    for (int i = 0; i < wastePile->count; ++i)
        playerState->hand[playerState->handCount++] = wastePile->pile[i];
    wastePile->count = 0;
    sort_hand_low_to_high(playerState);
}

/**
 * find_mirrored_card
 * When the top of the waste is a 3 (or Joker), return the last non-3/Joker
 * below it that acts as the “lock” being mirrored. Returns NULL if none.
 */
static Card* find_mirrored_card(CardPile *wastePile) {
    int idx = wastePile->count - 2;
    while (idx >= 0 &&
           (wastePile->pile[idx].is_joker || is_special_card(&wastePile->pile[idx], 3))) {
        --idx;
    }
    return (idx >= 0) ? &wastePile->pile[idx] : NULL;
}

/* --------------------------------------------------------------------------- */
/* SETUP & START-OF-GAME SWAPS                                                 */
/* --------------------------------------------------------------------------- */

/**
 * swap_hand_cards
 * Optional pre-game step: allow the player to swap any face-up card with a
 * hand card (one at a time, as many times as desired), then sort the hand.
 *
 * This helps players stage strong face-up cards for the mid/late game.
 */
static void swap_hand_cards(IdiotPlayer *playerState) {
    for (;;) {
        clear_screen();
        printf("--- Swap Cards ---\n\n");

        printf("Hand:      ");
        for (int i = 0; i < HAND_SIZE; ++i) { print_card_bracketed(&playerState->hand[i]); printf(" "); }
        printf("\nFace-up:   ");
        for (int i = 0; i < FACE_UP_SIZE; ++i) { print_card_bracketed(&playerState->faceUp[i]); printf(" "); }
        printf("\n\nReplace a face-up card?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");

        int menuChoice = 0;
        if (scanf("%d", &menuChoice) != 1 || menuChoice != 1) break;

        printf("\nWhich face-up card (1-3)? 4 = Cancel\n");
        printf("> ");
        int faceIndex = 0;
        scanf("%d", &faceIndex);
        if (faceIndex < 1 || faceIndex > 3) continue;
        --faceIndex;

        printf("\nWhich hand card (1-3)? 4 = Cancel\n");
        printf("> ");
        int handIndex = 0;
        scanf("%d", &handIndex);
        if (handIndex < 1 || handIndex > 3) continue;
        --handIndex;

        Card tmp                     = playerState->faceUp[faceIndex];
        playerState->faceUp[faceIndex] = playerState->hand[handIndex];
        playerState->hand[handIndex]   = tmp;
    }
    sort_hand_low_to_high(playerState);
}

/* --------------------------------------------------------------------------- */
/* AI UTILITIES                                                                */
/* --------------------------------------------------------------------------- */

/** Reset the AI turn summary prior to its move. */
static void lm_reset(AILastMove *summary) {
    summary->playedCount = 0;
    summary->burned      = 0;
    summary->mirrored    = 0;
    summary->mirroredCard = NULL;
}

/** Append a played card to the AI turn summary (capped). */
static void lm_record(AILastMove *summary, Card played) {
    if (summary->playedCount < LASTMOVE_MAX) {
        summary->played[summary->playedCount++] = played;
    }
}

/** Count duplicates of a rank currently in hand. */
static int duplicates_in_hand(const IdiotPlayer *playerState, const char *rankStr) {
    int count = 0;
    for (int i = 0; i < playerState->handCount; ++i)
        if (strcmp(playerState->hand[i].rank, rankStr) == 0) ++count;
    return count;
}

/**
 * top_run_length_by_value
 * Length (2..4) of the same-valued run ending at the top of the waste pile.
 */
static int top_run_length_by_value(const CardPile *pile) {
    if (pile->count == 0) return 0;
    int lastValue = card_value(&pile->pile[pile->count - 1]);
    int runLength = 1;
    for (int i = pile->count - 2; i >= 0 && runLength < 4; --i) {
        if (card_value(&pile->pile[i]) == lastValue) ++runLength;
        else break;
    }
    return runLength;
}

/**
 * should_burn_now_with_10
 * Conservative “is it worth using a 10 now?” policy:
 *   - Normal: prefer to burn only when the pile is large (>= 6).
 *   - Hard:   burn on large piles (>= 6) or when the top run is already 3.
 */
static int should_burn_now_with_10(const CardPile *pile, int difficulty) {
    int runLen = top_run_length_by_value(pile);
    int size   = pile->count;
    if (difficulty >= DIFFICULTY_HARD) return (size >= 6) || (runLen >= 3);
    return (size >= 6);
}

/**
 * count_playable_for_next
 * Rough ply-ahead: “If we pass the turn with this pile, how many replies does
 * the opponent have?” Counts legal plays from the next player’s current zone.
 */
static int count_playable_for_next(const CardPile *pile, const IdiotPlayer *nextPlayer) {
    const Card *top = (pile->count > 0) ? &pile->pile[pile->count - 1] : NULL;
    int count = 0;

    if (nextPlayer->handCount > 0) {
        for (int i = 0; i < nextPlayer->handCount; ++i)
            if (can_play_card(top, &nextPlayer->hand[i], pile)) ++count;
        return count;
    }

    if (nextPlayer->faceUpCount > 0) {
        for (int i = 0; i < nextPlayer->faceUpCount; ++i)
            if (can_play_card(top, &nextPlayer->faceUp[i], pile)) ++count;
        return count;
    }

    return (nextPlayer->faceDownCount > 0) ? 1 : 0;  /* At least a blind try. */
}

/**
 * hard_best_followup_index
 * After playing a 2, pick the best immediate follow-up (by simple score).
 * Returns the HAND index of the chosen card, or -1 if none are playable.
 */
static int hard_best_followup_index(const IdiotPlayer *aiState, const CardPile *pileAfterTwo) {
    int bestIndex = -1;
    int bestScore = -9999;

    for (int j = 0; j < aiState->handCount; ++j) {
        const Card *candidate = &aiState->hand[j];
        if (!can_play_card(&pileAfterTwo->pile[pileAfterTwo->count - 1], candidate, pileAfterTwo)) continue;

        int score = 0;
        if (is_special_card(candidate, 10)) score += 40;
        if (candidate->is_joker || is_special_card(candidate, 3)) score += 12;

        if (!candidate->is_joker &&
            !is_special_card(candidate, 2) &&
            !is_special_card(candidate, 3) &&
            !is_special_card(candidate, 10)) {
            score += 8 * (duplicates_in_hand(aiState, candidate->rank) - 1);
            if (card_value(candidate) >= 11) score += 6;
        }

        if (score > bestScore) { bestScore = score; bestIndex = j; }
    }
    return bestIndex;
}

/**
 * hard_score_candidate
 * Simulate one candidate play (either from hand or face-up when hand is empty)
 * and compute a greedy score that prefers:
 *   - burning big piles,
 *   - reducing opponent replies,
 *   - dumping duplicates,
 *   - conserving 10s unless valuable,
 *   - continuing after a 2 with a strong follow-up.
 *
 * We clone the involved states by value to keep this function side-effect free.
 */
static int hard_score_candidate(const IdiotPlayer *aiState,
                                const IdiotPlayer *opponentState,
                                const CardPile    *pileState,
                                const CardPile    *drawPileState,
                                int                fromHandZone,   /* 1=hand, 0=face-up */
                                int                indexInZone)
{
    (void)drawPileState; /* Not used in this heuristic; draw is handled at play time. */

    IdiotPlayer ai    = *aiState;
    IdiotPlayer opp   = *opponentState;
    CardPile    pile  = *pileState;

    Card played = fromHandZone ? ai.hand[indexInZone] : ai.faceUp[indexInZone];

    if (fromHandZone) {
        for (int j = indexInZone; j < ai.handCount - 1; ++j) ai.hand[j] = ai.hand[j + 1];
        ai.handCount--;
    } else {
        for (int j = indexInZone; j < ai.faceUpCount - 1; ++j) ai.faceUp[j] = ai.faceUp[j + 1];
        ai.faceUpCount--;
    }

    pile.pile[pile.count++] = played;

    int burned = 0;

    if (is_special_card(&played, 10)) {
        pile.count = 0;
        burned     = 1;
    } else if (is_special_card(&played, 2)) {
        CardPile afterTwo = pile;
        int idx = hard_best_followup_index(&ai, &afterTwo);
        if (idx != -1) {
            Card follow = ai.hand[idx];
            for (int k = idx; k < ai.handCount - 1; ++k) ai.hand[k] = ai.hand[k + 1];
            ai.handCount--;
            afterTwo.pile[afterTwo.count++] = follow;

            if (is_special_card(&follow, 10)) {
                afterTwo.count = 0;
                burned         = 1;
            } else if (!follow.is_joker &&
                       !is_special_card(&follow, 2) &&
                       !is_special_card(&follow, 3) &&
                       !is_special_card(&follow, 10)) {
                int dumped = 0;
                for (int i = 0; i < ai.handCount && dumped < 3; ) {
                    if (strcmp(ai.hand[i].rank, follow.rank) == 0) {
                        afterTwo.pile[afterTwo.count++] = ai.hand[i];
                        for (int k = i; k < ai.handCount - 1; ++k) ai.hand[k] = ai.hand[k + 1];
                        ai.handCount--; dumped++;
                        if (is_four_of_a_kind(&afterTwo)) { afterTwo.count = 0; burned = 1; break; }
                    } else {
                        ++i;
                    }
                }
            }
            pile = afterTwo;
        }
    } else if (!played.is_joker && !is_special_card(&played, 3)) {
        /* Dump duplicates of a normal card from the same zone. */
        int dumped = 0;
        if (fromHandZone) {
            for (int i = 0; i < ai.handCount && dumped < 3; ) {
                if (strcmp(ai.hand[i].rank, played.rank) == 0) {
                    pile.pile[pile.count++] = ai.hand[i];
                    for (int k = i; k < ai.handCount - 1; ++k) ai.hand[k] = ai.hand[k + 1];
                    ai.handCount--; dumped++;
                    if (is_four_of_a_kind(&pile)) { pile.count = 0; burned = 1; break; }
                } else {
                    ++i;
                }
            }
        } else {
            for (int i = 0; i < ai.faceUpCount && dumped < 3; ) {
                if (strcmp(ai.faceUp[i].rank, played.rank) == 0) {
                    pile.pile[pile.count++] = ai.faceUp[i];
                    for (int k = i; k < ai.faceUpCount - 1; ++k) ai.faceUp[k] = ai.faceUp[k + 1];
                    ai.faceUpCount--; dumped++;
                    if (is_four_of_a_kind(&pile)) { pile.count = 0; burned = 1; break; }
                } else {
                    ++i;
                }
            }
        }
    }

    /* Greedy score: fewer replies from opponent is good; burns are great. */
    int replies = count_playable_for_next(&pile, &opp);
    int score   = (replies == 0 ? 1000 : -8 * replies) + (burned ? 200 : 0);

    /* Leaving a high lock with no escape in opponent’s hand is a bonus. */
    if (pile.count > 0 && opp.handCount > 0) {
        const Card *lock = &pile.pile[pile.count - 1];
        int oppHasEscape = 0;
        for (int i = 0; i < opp.handCount; ++i) {
            const Card *pc = &opp.hand[i];
            if (pc->is_joker || is_special_card(pc, 2) || is_special_card(pc, 3) || is_special_card(pc, 10)) {
                oppHasEscape = 1; break;
            }
        }
        if (!oppHasEscape && card_value(lock) >= 11) score += 30;
    }

    /* Prefer moves that reduce our total cards. */
    int ourBefore = aiState->handCount + aiState->faceUpCount + aiState->faceDownCount;
    int ourAfter  = ai.handCount    + ai.faceUpCount    + ai.faceDownCount;
    score += (ourBefore - ourAfter) * 6;

    /* Discourage wasting a 10 on tiny piles. */
    if (is_special_card(&played, 10) && pileState->count < 3) score -= 25;

    return score;
}

/** Remove and return arr[index], shifting the remainder left. */
static Card take_at(Card *arr, int *countRef, int index) {
    Card c = arr[index];
    for (int j = index; j < *countRef - 1; ++j) arr[j] = arr[j + 1];
    (*countRef)--;
    return c;
}

/**
 * dump_same_rank_in_hand
 * After seeding a normal card, dump up to capToPlay more of the same rank from
 * the AI’s hand to accelerate toward a burn. Returns the number added.
 */
static int dump_same_rank_in_hand(IdiotPlayer *aiState,
                                  CardPile    *pile,
                                  const char  *rankStr,
                                  int          capToPlay,
                                  AILastMove  *summary)
{
    int playedExtra = 0;
    for (int i = 0; i < aiState->handCount && playedExtra < capToPlay; ) {
        if (strcmp(aiState->hand[i].rank, rankStr) == 0) {
            pile->pile[pile->count++] = aiState->hand[i];
            if (summary) lm_record(summary, aiState->hand[i]);
            for (int j = i; j < aiState->handCount - 1; ++j) aiState->hand[j] = aiState->hand[j + 1];
            aiState->handCount--; playedExtra++;
        } else {
            ++i;
        }
    }
    return playedExtra;
}

/* --------------------------------------------------------------------------- */
/* AI TURN DRIVER                                                              */
/* --------------------------------------------------------------------------- */

/**
 * ai_play
 * Drive a single AI turn according to difficulty:
 *
 * EASY:
 *   - Timid, prioritizes non-power plays, then 3, then 2, then 10.
 *   - Uses face-up cards only after the hand empties; otherwise picks up.
 *
 * NORMAL:
 *   - If a 2 is available/usable: play it and immediately follow with the
 *     lowest non-power (or 3, or 10 on large piles), dumping duplicates.
 *   - Otherwise: lowest non-power (+dump), else 3, else 10 (only if worth it).
 *
 * HARD:
 *   - Greedy look-ahead scoring of all candidates (hand first, then face-up
 *     only if hand is empty), with bonuses for burns, duplicate dumps, and
 *     limiting opponent replies. Chains after a 2.
 */
static void ai_play(IdiotPlayer *aiState,
                    IdiotPlayer *opponentState,
                    CardPile    *wastePile,
                    CardPile    *drawPile,
                    int          difficulty,
                    AILastMove  *aiLastTurnSummary)
{
    Card *topOfWaste = (wastePile->count > 0) ? &wastePile->pile[wastePile->count - 1] : NULL;
    lm_reset(aiLastTurnSummary);

    /* -------------------- EASY -------------------- */
    if (difficulty == DIFFICULTY_EASY) {
        /* 1) Hand: non-power playable */
        for (int i = 0; i < aiState->handCount; ++i) {
            if (is_power_card(&aiState->hand[i])) continue;
            if (!can_play_card(topOfWaste, &aiState->hand[i], wastePile)) continue;
            Card played = take_at(aiState->hand, &aiState->handCount, i);
            wastePile->pile[wastePile->count++] = played;
            lm_record(aiLastTurnSummary, played);
            draw_from_pile(aiState, drawPile);
            if (is_special_card(&played, 10)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); }
            if (is_special_card(&played, 3))  { aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile); }
            return;
        }
        /* 2) Hand: 3, then 2, then 10 */
        for (int i = 0; i < aiState->handCount; ++i) {
            if (is_special_card(&aiState->hand[i], 3) && can_play_card(topOfWaste, &aiState->hand[i], wastePile)) {
                Card played = take_at(aiState->hand, &aiState->handCount, i);
                wastePile->pile[wastePile->count++] = played;
                lm_record(aiLastTurnSummary, played);
                aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile);
                draw_from_pile(aiState, drawPile);
                return;
            }
        }
        for (int i = 0; i < aiState->handCount; ++i) {
            if (is_special_card(&aiState->hand[i], 2) && can_play_card(topOfWaste, &aiState->hand[i], wastePile)) {
                Card played = take_at(aiState->hand, &aiState->handCount, i);
                wastePile->pile[wastePile->count++] = played;
                lm_record(aiLastTurnSummary, played);
                draw_from_pile(aiState, drawPile);
                return; /* main loop grants another turn after a 2 */
            }
        }
        for (int i = 0; i < aiState->handCount; ++i) {
            if (is_special_card(&aiState->hand[i], 10) && can_play_card(topOfWaste, &aiState->hand[i], wastePile)) {
                Card played = take_at(aiState->hand, &aiState->handCount, i);
                wastePile->pile[wastePile->count++] = played;
                lm_record(aiLastTurnSummary, played);
                aiLastTurnSummary->burned = 1; burn_pile(wastePile);
                draw_from_pile(aiState, drawPile);
                return;
            }
        }
        /* 3) Face-up fallback (same order) when hand is empty */
        if (aiState->handCount == 0 && aiState->faceUpCount > 0) {
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (is_power_card(&aiState->faceUp[i])) continue;
                if (!can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) continue;
                Card played = take_at(aiState->faceUp, &aiState->faceUpCount, i);
                wastePile->pile[wastePile->count++] = played;
                lm_record(aiLastTurnSummary, played);
                return;
            }
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (is_special_card(&aiState->faceUp[i], 3) && can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) {
                    Card played = take_at(aiState->faceUp, &aiState->faceUpCount, i);
                    wastePile->pile[wastePile->count++] = played;
                    lm_record(aiLastTurnSummary, played);
                    aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile);
                    return;
                }
            }
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (is_special_card(&aiState->faceUp[i], 2) && can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) {
                    Card played = take_at(aiState->faceUp, &aiState->faceUpCount, i);
                    wastePile->pile[wastePile->count++] = played;
                    lm_record(aiLastTurnSummary, played);
                    return;
                }
            }
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (is_special_card(&aiState->faceUp[i], 10) && can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) {
                    Card played = take_at(aiState->faceUp, &aiState->faceUpCount, i);
                    wastePile->pile[wastePile->count++] = played;
                    lm_record(aiLastTurnSummary, played);
                    aiLastTurnSummary->burned = 1; burn_pile(wastePile);
                    return;
                }
            }
        }
        /* 4) Face-down phase (blind try), else pick up */
        if (aiState->handCount == 0 && aiState->faceUpCount == 0 && aiState->faceDownCount > 0) {
            Card blind = take_at(aiState->faceDown, &aiState->faceDownCount, 0);
            if (can_play_card(topOfWaste, &blind, wastePile)) {
                wastePile->pile[wastePile->count++] = blind;
                lm_record(aiLastTurnSummary, blind);
                if (is_special_card(&blind, 10) || is_four_of_a_kind(wastePile)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); }
                if (is_special_card(&blind, 3)) { aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile); }
                return;
            } else {
                for (int i = 0; i < wastePile->count; ++i) aiState->hand[aiState->handCount++] = wastePile->pile[i];
                aiState->hand[aiState->handCount++] = blind;
                wastePile->count = 0;
                lm_reset(aiLastTurnSummary);
                return;
            }
        }
        /* 5) Nothing playable → pick up */
        for (int i = 0; i < wastePile->count; ++i) aiState->hand[aiState->handCount++] = wastePile->pile[i];
        wastePile->count = 0;
        return;
    }

    /* -------------------- NORMAL -------------------- */
    if (difficulty == DIFFICULTY_NORMAL) {
        int idxTwo = -1, idxTen = -1, idxThree = -1, idxLowest = -1, lowestVal = 1000;

        /* Scan the hand for candidates. */
        for (int i = 0; i < aiState->handCount; ++i) {
            if (!can_play_card(topOfWaste, &aiState->hand[i], wastePile)) continue;
            int v = card_value(&aiState->hand[i]);
            if (is_power_card(&aiState->hand[i]) == 0 && v < lowestVal) { lowestVal = v; idxLowest = i; }
            if (is_special_card(&aiState->hand[i], 2))  idxTwo  = i;
            if (is_special_card(&aiState->hand[i], 3))  idxThree = i;
            if (is_special_card(&aiState->hand[i], 10)) idxTen   = i;
        }

        /* 2 then follow-up (lowest non-power preferred; else 3; else 10 on large piles) */
        if (idxTwo != -1) {
            Card two = take_at(aiState->hand, &aiState->handCount, idxTwo);
            wastePile->pile[wastePile->count++] = two;
            lm_record(aiLastTurnSummary, two);
            draw_from_pile(aiState, drawPile);

            Card *topAfterTwo = &wastePile->pile[wastePile->count - 1];
            int idxNextLow = -1, idxNextThree = -1, idxNextTen = -1, lowV = 1000;

            for (int i = 0; i < aiState->handCount; ++i) {
                if (!can_play_card(topAfterTwo, &aiState->hand[i], wastePile)) continue;
                int v = card_value(&aiState->hand[i]);
                if (is_power_card(&aiState->hand[i]) == 0 && v < lowV) { lowV = v; idxNextLow = i; }
                if (is_special_card(&aiState->hand[i], 3))  idxNextThree = i;
                if (is_special_card(&aiState->hand[i], 10)) idxNextTen   = i;
            }

            if (idxNextLow != -1) {
                Card n = take_at(aiState->hand, &aiState->handCount, idxNextLow);
                wastePile->pile[wastePile->count++] = n;
                lm_record(aiLastTurnSummary, n);
                (void)dump_same_rank_in_hand(aiState, wastePile, n.rank, 3, aiLastTurnSummary);
                draw_from_pile(aiState, drawPile);
            } else if (idxNextThree != -1) {
                Card n = take_at(aiState->hand, &aiState->handCount, idxNextThree);
                wastePile->pile[wastePile->count++] = n;
                lm_record(aiLastTurnSummary, n);
                aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile);
                draw_from_pile(aiState, drawPile);
            } else if (idxNextTen != -1 && should_burn_now_with_10(wastePile, difficulty)) {
                Card n = take_at(aiState->hand, &aiState->handCount, idxNextTen);
                wastePile->pile[wastePile->count++] = n;
                lm_record(aiLastTurnSummary, n);
                aiLastTurnSummary->burned = 1; burn_pile(wastePile);
                draw_from_pile(aiState, drawPile);
            }
            return;
        }

        /* Lowest non-power (+dump), else 3, else 10 if worth it. */
        if (idxLowest != -1) {
            Card c = take_at(aiState->hand, &aiState->handCount, idxLowest);
            wastePile->pile[wastePile->count++] = c;
            lm_record(aiLastTurnSummary, c);
            (void)dump_same_rank_in_hand(aiState, wastePile, c.rank, 3, aiLastTurnSummary);
            draw_from_pile(aiState, drawPile);
            return;
        }

        if (idxThree != -1) {
            Card c = take_at(aiState->hand, &aiState->handCount, idxThree);
            wastePile->pile[wastePile->count++] = c;
            lm_record(aiLastTurnSummary, c);
            aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile);
            draw_from_pile(aiState, drawPile);
            return;
        }

        if (idxTen != -1 && should_burn_now_with_10(wastePile, difficulty)) {
            Card c = take_at(aiState->hand, &aiState->handCount, idxTen);
            wastePile->pile[wastePile->count++] = c;
            lm_record(aiLastTurnSummary, c);
            aiLastTurnSummary->burned = 1; burn_pile(wastePile);
            draw_from_pile(aiState, drawPile);
            return;
        }

        /* Face-up mirror of the above (no drawing). */
        if (aiState->handCount == 0 && aiState->faceUpCount > 0) {
            int fuTwo = -1, fuTen = -1, fuThree = -1, fuLow = -1, fuLowV = 1000;
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (!can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) continue;
                int v = card_value(&aiState->faceUp[i]);
                if (is_power_card(&aiState->faceUp[i]) == 0 && v < fuLowV) { fuLowV = v; fuLow = i; }
                if (is_special_card(&aiState->faceUp[i], 2))  fuTwo  = i;
                if (is_special_card(&aiState->faceUp[i], 3))  fuThree = i;
                if (is_special_card(&aiState->faceUp[i], 10)) fuTen   = i;
            }
            if (fuTwo   != -1) { Card c = take_at(aiState->faceUp, &aiState->faceUpCount, fuTwo);   wastePile->pile[wastePile->count++] = c; lm_record(aiLastTurnSummary, c); return; }
            if (fuLow   != -1) { Card c = take_at(aiState->faceUp, &aiState->faceUpCount, fuLow);   wastePile->pile[wastePile->count++] = c; lm_record(aiLastTurnSummary, c); return; }
            if (fuThree != -1) { Card c = take_at(aiState->faceUp, &aiState->faceUpCount, fuThree); wastePile->pile[wastePile->count++] = c; lm_record(aiLastTurnSummary, c); aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile); return; }
            if (fuTen   != -1 && should_burn_now_with_10(wastePile, difficulty)) {
                Card c = take_at(aiState->faceUp, &aiState->faceUpCount, fuTen);
                wastePile->pile[wastePile->count++] = c; lm_record(aiLastTurnSummary, c);
                aiLastTurnSummary->burned = 1; burn_pile(wastePile);
                return;
            }
        }

        /* Face-down blind try, else pick up. */
        if (aiState->handCount == 0 && aiState->faceUpCount == 0 && aiState->faceDownCount > 0) {
            Card blind = take_at(aiState->faceDown, &aiState->faceDownCount, 0);
            if (can_play_card(topOfWaste, &blind, wastePile)) {
                wastePile->pile[wastePile->count++] = blind; lm_record(aiLastTurnSummary, blind);
                if (is_special_card(&blind, 10) || is_four_of_a_kind(wastePile)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); }
                if (is_special_card(&blind, 3)) { aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile); }
                return;
            } else {
                for (int i = 0; i < wastePile->count; ++i) aiState->hand[aiState->handCount++] = wastePile->pile[i];
                aiState->hand[aiState->handCount++] = blind;
                wastePile->count = 0;
                lm_reset(aiLastTurnSummary);
                return;
            }
        }

        for (int i = 0; i < wastePile->count; ++i) aiState->hand[aiState->handCount++] = wastePile->pile[i];
        wastePile->count = 0;
        return;
    }

    /* -------------------- HARD -------------------- */
    if (difficulty == DIFFICULTY_HARD) {
        int fromHandZone = -1, bestIndex = -1, bestScore = -999999;

        /* Prefer hand candidates; if none, try face-up (only when hand is empty). */
        if (aiState->handCount > 0) {
            for (int i = 0; i < aiState->handCount; ++i) {
                if (!can_play_card(topOfWaste, &aiState->hand[i], wastePile)) continue;
                int score = hard_score_candidate(aiState, opponentState, wastePile, drawPile, 1, i);
                if (score > bestScore) { bestScore = score; bestIndex = i; fromHandZone = 1; }
            }
        }
        if (fromHandZone == -1 && aiState->faceUpCount > 0 && aiState->handCount == 0) {
            for (int i = 0; i < aiState->faceUpCount; ++i) {
                if (!can_play_card(topOfWaste, &aiState->faceUp[i], wastePile)) continue;
                int score = hard_score_candidate(aiState, opponentState, wastePile, drawPile, 0, i);
                if (score > bestScore) { bestScore = score; bestIndex = i; fromHandZone = 0; }
            }
        }
        /* Nothing playable → pick up. */
        if (fromHandZone == -1) {
            for (int i = 0; i < wastePile->count; ++i) aiState->hand[aiState->handCount++] = wastePile->pile[i];
            wastePile->count = 0;
            return;
        }

        /* Execute chosen play. */
        Card played = fromHandZone ? aiState->hand[bestIndex] : aiState->faceUp[bestIndex];
        if (fromHandZone) { for (int j = bestIndex; j < aiState->handCount - 1; ++j) aiState->hand[j] = aiState->hand[j + 1]; aiState->handCount--; }
        else              { for (int j = bestIndex; j < aiState->faceUpCount - 1; ++j) aiState->faceUp[j] = aiState->faceUp[j + 1]; aiState->faceUpCount--; }

        wastePile->pile[wastePile->count++] = played;
        lm_record(aiLastTurnSummary, played);

        if (is_special_card(&played, 10)) {
            aiLastTurnSummary->burned = 1; burn_pile(wastePile); draw_from_pile(aiState, drawPile); return;
        }

        if (!played.is_joker && !is_special_card(&played, 2) && !is_special_card(&played, 3)) {
            int dumped = 0;
            if (fromHandZone) {
                for (int i = 0; i < aiState->handCount && dumped < 3; ) {
                    if (strcmp(aiState->hand[i].rank, played.rank) == 0) {
                        wastePile->pile[wastePile->count++] = aiState->hand[i];
                        lm_record(aiLastTurnSummary, aiState->hand[i]);
                        for (int k = i; k < aiState->handCount - 1; ++k) aiState->hand[k] = aiState->hand[k + 1];
                        aiState->handCount--; dumped++;
                        if (is_four_of_a_kind(wastePile)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); draw_from_pile(aiState, drawPile); return; }
                    } else ++i;
                }
            } else {
                for (int i = 0; i < aiState->faceUpCount && dumped < 3; ) {
                    if (strcmp(aiState->faceUp[i].rank, played.rank) == 0) {
                        wastePile->pile[wastePile->count++] = aiState->faceUp[i];
                        lm_record(aiLastTurnSummary, aiState->faceUp[i]);
                        for (int k = i; k < aiState->faceUpCount - 1; ++k) aiState->faceUp[k] = aiState->faceUp[k + 1];
                        aiState->faceUpCount--; dumped++;
                        if (is_four_of_a_kind(wastePile)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); return; }
                    } else ++i;
                }
            }
            draw_from_pile(aiState, drawPile);
            return;
        }

        if (is_special_card(&played, 3) || played.is_joker) {
            aiLastTurnSummary->mirrored = 1;
            aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile);
            draw_from_pile(aiState, drawPile);
            return;
        }

        if (is_special_card(&played, 2)) {
            draw_from_pile(aiState, drawPile);
            int j = hard_best_followup_index(aiState, wastePile);
            if (j != -1 && can_play_card(&wastePile->pile[wastePile->count - 1], &aiState->hand[j], wastePile)) {
                Card follow = aiState->hand[j];
                for (int k = j; k < aiState->handCount - 1; ++k) aiState->hand[k] = aiState->hand[k + 1];
                aiState->handCount--;
                wastePile->pile[wastePile->count++] = follow; lm_record(aiLastTurnSummary, follow);

                if (is_special_card(&follow, 10)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); draw_from_pile(aiState, drawPile); return; }
                if (is_special_card(&follow, 3) || follow.is_joker) { aiLastTurnSummary->mirrored = 1; aiLastTurnSummary->mirroredCard = find_mirrored_card(wastePile); draw_from_pile(aiState, drawPile); return; }

                if (!follow.is_joker && !is_special_card(&follow, 2) && !is_special_card(&follow, 3) && !is_special_card(&follow, 10)) {
                    int dumped = 0;
                    for (int i = 0; i < aiState->handCount && dumped < 3; ) {
                        if (strcmp(aiState->hand[i].rank, follow.rank) == 0) {
                            wastePile->pile[wastePile->count++] = aiState->hand[i];
                            lm_record(aiLastTurnSummary, aiState->hand[i]);
                            for (int k = i; k < aiState->handCount - 1; ++k) aiState->hand[k] = aiState->hand[k + 1];
                            aiState->handCount--; dumped++;
                            if (is_four_of_a_kind(wastePile)) { aiLastTurnSummary->burned = 1; burn_pile(wastePile); draw_from_pile(aiState, drawPile); return; }
                        } else ++i;
                    }
                }
                draw_from_pile(aiState, drawPile);
            }
            return;
        }

        draw_from_pile(aiState, drawPile);
        return;
    }
}

/* --------------------------------------------------------------------------- */
/* RENDERING                                                                   */
/* --------------------------------------------------------------------------- */

/**
 * display_idiot_game
 * Clear the screen and present a snapshot of the current state, including:
 *   - AI’s last move (if any),
 *   - AI’s face-down/face-up stacks and hidden hand size,
 *   - draw pile size,
 *   - waste pile top (and mirrored lock if top is a 3),
 *   - player’s hand, face-up, and face-down stacks.
 */
static void display_idiot_game(IdiotPlayer *playerState,
                               IdiotPlayer *opponentState,
                               CardPile    *drawPile,
                               CardPile    *wastePile,
                               AILastMove  *aiLastTurnSummary)
{
    const int maxHiddenHandPreview = 6;

    clear_screen();

    /* AI last move summary (if any). */
    if (aiLastTurnSummary &&
        (aiLastTurnSummary->playedCount > 0 || aiLastTurnSummary->burned || aiLastTurnSummary->mirrored))
    {
        for (int i = 0; i < aiLastTurnSummary->playedCount; ++i) {
            printf("Opponent played ");
            print_card_bracketed(&aiLastTurnSummary->played[i]);
            if (is_special_card(&aiLastTurnSummary->played[i], 3)) {
                if (aiLastTurnSummary->mirroredCard) {
                    printf(" (Mirroring: ");
                    print_card_bracketed(aiLastTurnSummary->mirroredCard);
                    printf(")");
                } else {
                    printf(" (Mirroring: [none])");
                }
            }
            printf("\n");
        }
        if (aiLastTurnSummary->burned) {
            printf("Opponent burned the pile!\n");
        }
        if (aiLastTurnSummary->playedCount == 0) {
            printf("Opponent takes the pile.\n");
        }
    }

    /* Opponent view. */
    printf("\n--- Opponent ---\n");
    print_hidden_brackets(opponentState->faceDownCount);
    printf("\n");
    for (int i = 0; i < opponentState->faceUpCount; ++i) { print_card_bracketed(&opponentState->faceUp[i]); printf(" "); }
    printf("\n");
    if (opponentState->handCount > maxHiddenHandPreview) {
        print_hidden_brackets(maxHiddenHandPreview);
        printf("...\n");
    } else {
        print_hidden_brackets(opponentState->handCount);
        printf("\n");
    }
    printf("(%d cards in hand)\n\n", opponentState->handCount);

    /* Piles. */
    printf("Draw Pile: %d cards\n", drawPile->count);

    if (wastePile->count > 0) {
        Card *top = &wastePile->pile[wastePile->count - 1];
        printf("Waste Pile: ");
        print_card_bracketed(top);
        printf("\n");
        if (is_special_card(top, 3)) {
            Card *lock = find_mirrored_card(wastePile);
            if (lock) {
                printf("   (Mirroring: ");
                print_card_bracketed(lock);
                printf(")\n");
            } else {
                printf("   (Mirroring: [none])\n");
            }
        }
    } else {
        printf("Waste Pile: [empty]\n");
    }

    /* Player view. */
    printf("\n--- Your Hand ---\n");
    for (int i = 0; i < playerState->handCount; ++i) { print_card_bracketed(&playerState->hand[i]); printf(" "); }
    printf("\n\n");
    for (int i = 0; i < playerState->faceUpCount; ++i) { print_card_bracketed(&playerState->faceUp[i]); printf(" "); }
    printf("\n");
    print_hidden_brackets(playerState->faceDownCount);
    printf("\n\n");
}

/* --------------------------------------------------------------------------- */
/* PUBLIC UI: RULE PAGE                                                        */
/* --------------------------------------------------------------------------- */

/**
 * idiotHowToPlay
 * Clear the screen and print a short rule/tutorial page for Idiot.
 * Blocks until the user presses Enter.
 */
void idiotHowToPlay(void) {
    clear_screen();
    printf("=== HOW TO PLAY: IDIOT ===\n\n");

    printf("- Each player has 3 face-down, 3 face-up, and 3 hand cards.\n");
    printf("- Take turns playing cards onto the waste pile.\n");
    printf("- Card must be equal or higher in value than the top card.\n");
    printf("- Special cards:\n");
    printf("    2   resets the pile (anything can be played next)\n");
    printf("    3   mirrors the last non-3/Joker below the top\n");
    printf("    10  burns the pile (removes all cards)\n");
    printf("- Four of the same value in a row also burns the pile.\n");
    printf("- Jokers act exactly like a 3 and can be played at any time.\n");
    printf("- If you cannot play, you must take the entire pile.\n");
    printf("- First to play all cards wins.\n\n");

    pause_for_enter();
    clear_screen();
}

/* --------------------------------------------------------------------------- */
/* PUBLIC ENTRY POINT                                                          */
/* --------------------------------------------------------------------------- */

/**
 * idiot_start
 * Top-level entry point for the Idiot game mode. Handles:
 *   - difficulty selection and optional betting,
 *   - dealing (with optional Jokers inserted into the draw pile),
 *   - the interactive loop (player vs AI),
 *   - payout and stat updates when the game ends.
 */
void idiot_start(void) {
    /* --- Allocate and initialize top-level state containers --- */
    Card         deck[DECK_SIZE];
    IdiotPlayer  playerState           = {0};
    IdiotPlayer  aiState               = {0};
    CardPile     drawPile              = {0};
    CardPile     wastePile             = {0};
    AILastMove   aiLastTurnSummary     = {0};

    int          difficultyChoice      = 0;
    int          currentPlayerIndex    = 0;   /* 0 = human, 1 = AI */
    int          tricksterWinEligible  = 1;   /* invalidated if player picks up */

    /* --- Difficulty selection --- */
    clear_screen();
    printf("Select Opponent Difficulty:\n");
    printf("1. Easy\n");
    printf("2. Normal\n");
    printf("3. Hard\n");
    printf("> ");
    scanf("%d", &difficultyChoice);

    /* --- Betting (Normal/Hard only) --- */
    unsigned int wagerAmount           = 0;
    const unsigned int minWager        = 10;
    const unsigned int maxWager        = (difficultyChoice == DIFFICULTY_NORMAL) ? 100 : 500;

    if (difficultyChoice != DIFFICULTY_EASY) {
        printf("Place your bet ($%u - $%u): ", minWager, maxWager);
        for (;;) {
            scanf("%u", &wagerAmount);
            if (wagerAmount >= minWager &&
                wagerAmount <= maxWager &&
                wagerAmount <= playerData.uPlayerMoney) break;
            printf("Invalid bet. Enter a value between $%u and $%u: ", minWager, maxWager);
        }
        playerData.uPlayerMoney -= wagerAmount;  /* Deduct up-front. */
    }

    /* --- Deal --- */
    initialize_deck(deck);
    shuffle_deck(deck);

    /* Face-down. */
    for (int i = 0; i < FACE_DOWN_SIZE; ++i) {
        playerState.faceDown[i] = deck[i];
        aiState.faceDown[i]     = deck[i + FACE_DOWN_SIZE];
    }
    /* Face-up. */
    for (int i = 0; i < FACE_UP_SIZE; ++i) {
        playerState.faceUp[i] = deck[i + 2 * FACE_DOWN_SIZE];
        playerState.faceUp[i].revealed = 1;
        aiState.faceUp[i]     = deck[i + 2 * FACE_DOWN_SIZE + FACE_UP_SIZE];
        aiState.faceUp[i].revealed     = 1;
    }
    /* Hands. */
    for (int i = 0; i < HAND_SIZE; ++i) {
        playerState.hand[i] = deck[i + 2 * FACE_DOWN_SIZE + 2 * FACE_UP_SIZE];
        aiState.hand[i]     = deck[i + 2 * FACE_DOWN_SIZE + 2 * FACE_UP_SIZE + HAND_SIZE];
    }

    playerState.handCount     = HAND_SIZE;
    playerState.faceUpCount   = FACE_UP_SIZE;
    playerState.faceDownCount = FACE_DOWN_SIZE;

    aiState.handCount         = HAND_SIZE;
    aiState.faceUpCount       = FACE_UP_SIZE;
    aiState.faceDownCount     = FACE_DOWN_SIZE;

    /* Draw pile: remaining cards. */
    for (int i = 18; i < DECK_SIZE; ++i) {
        drawPile.pile[drawPile.count++] = deck[i];
    }

    /* Optional: insert two Jokers randomly into the draw pile. */
    if (config.jokers) {
        Card jokerTemplate = { .suit = "Joker", .rank = "Joker", .revealed = 1, .is_joker = 1 };
        for (int n = 0; n < 2; ++n) {
            int insertPos = (drawPile.count == 0) ? 0 : rand() % (drawPile.count + 1);
            for (int k = drawPile.count; k > insertPos; --k) drawPile.pile[k] = drawPile.pile[k - 1];
            drawPile.pile[insertPos] = jokerTemplate;
            drawPile.count++;
        }
    }

    /* Allow the human to stage their face-up cards. */
    swap_hand_cards(&playerState);

    /* First turn bias by difficulty (EASY → player starts, HARD → AI starts). */
    currentPlayerIndex =
        (difficultyChoice == DIFFICULTY_EASY)  ? 0 :
        (difficultyChoice == DIFFICULTY_HARD)  ? 1 :
                                            rand() % 2;

    /* --- Main game loop --- */
    for (;;) {
        display_idiot_game(&playerState, &aiState, &drawPile, &wastePile, &aiLastTurnSummary);

        IdiotPlayer *turnPlayer = (currentPlayerIndex == 0) ? &playerState : &aiState;
        Card        *topOfWaste = (wastePile.count > 0) ? &wastePile.pile[wastePile.count - 1] : NULL;

        /* Determine how many playable positions we should show this player. */
        int playableCount = turnPlayer->handCount;
        if (turnPlayer->handCount == 0 && turnPlayer->faceUpCount > 0) {
            playableCount = turnPlayer->faceUpCount;
        } else if (turnPlayer->handCount == 0 && turnPlayer->faceUpCount == 0 && drawPile.count == 0) {
            if (turnPlayer->faceDownCount > 0) {
                if (currentPlayerIndex == 0) {
                    printf("\nNo hand/face-up cards left. You may now play your face-down cards.\n");
                }
                playableCount = turnPlayer->faceDownCount;
            } else {
                /* Active player has no cards anywhere → they win. */
                printf("\n%s wins the game!\n", (currentPlayerIndex == 0) ? "Player" : "Opponent");
                if (currentPlayerIndex == 0) {
                    unsigned int payoutMultiplier =
                        (difficultyChoice == DIFFICULTY_NORMAL) ? 2 :
                        (difficultyChoice == DIFFICULTY_HARD)   ? 5 : 1;
                    playerData.uPlayerMoney += wagerAmount * payoutMultiplier;
                    playerData.idiot.wins++;
                    playerData.idiot.win_streak++;
                    if (playerData.idiot.max_win_streak <= playerData.idiot.win_streak)
                        playerData.idiot.max_win_streak = playerData.idiot.win_streak;
                } else {
                    playerData.idiot.losses++;
                    playerData.idiot.win_streak = 0;
                }
                break;
            }
        }

        /* ---------------- Human turn ---------------- */
        if (currentPlayerIndex == 0) {
            int selectionIndex = -1;

            /* 0 = take pile. Otherwise select a card position (1..playableCount). */
            for (;;) {
                printf("\nYour turn. Select card to play (1-%d), or 0 to take pile: ", playableCount);
                if (scanf("%d", &selectionIndex) != 1) {
                    while (getchar() != '\n');      /* clear invalid input */
                    printf("Invalid selection.\n");
                    continue;
                }
                if (selectionIndex < 0 || selectionIndex > playableCount) {
                    printf("Invalid selection.\n");
                    continue;
                }
                break;
            }

            /* Take pile? */
            if (selectionIndex == 0) {
                handle_pile_pickup(turnPlayer, &wastePile);
                tricksterWinEligible = 0;           /* No trickster if player picked up. */
                continue;                            /* Player gets another turn. */
            }

            Card selectedCard;
            int  moveIsValid       = 0;
            int  alreadyCommitted  = 0;            /* Set if we already pushed to waste in face-down flow. */

            /* Hand phase */
            if (turnPlayer->handCount > 0 && selectionIndex <= turnPlayer->handCount) {
                selectedCard = turnPlayer->hand[selectionIndex - 1];
                moveIsValid  = (!topOfWaste || can_play_card(topOfWaste, &selectedCard, &wastePile));
                if (moveIsValid) {
                    for (int j = selectionIndex - 1; j < turnPlayer->handCount - 1; ++j)
                        turnPlayer->hand[j] = turnPlayer->hand[j + 1];
                    turnPlayer->handCount--;
                }
            }
            /* Face-up phase */
            else if (turnPlayer->faceUpCount > 0 && selectionIndex <= turnPlayer->faceUpCount) {
                selectedCard = turnPlayer->faceUp[selectionIndex - 1];
                moveIsValid  = (!topOfWaste || can_play_card(topOfWaste, &selectedCard, &wastePile));
                if (moveIsValid) {
                    for (int j = selectionIndex - 1; j < turnPlayer->faceUpCount - 1; ++j)
                        turnPlayer->faceUp[j] = turnPlayer->faceUp[j + 1];
                    turnPlayer->faceUpCount--;
                }
            }
            /* Face-down phase (always reveal the chosen card) */
            else if (turnPlayer->faceDownCount > 0) {
                selectedCard = turnPlayer->faceDown[selectionIndex - 1];
                for (int j = selectionIndex - 1; j < turnPlayer->faceDownCount - 1; ++j)
                    turnPlayer->faceDown[j] = turnPlayer->faceDown[j + 1];
                turnPlayer->faceDownCount--;

                int faceDownValid = (!topOfWaste || can_play_card(topOfWaste, &selectedCard, &wastePile));
                if (faceDownValid) {
                    wastePile.pile[wastePile.count++] = selectedCard;   /* commit now */
                    draw_from_pile(turnPlayer, &drawPile);
                    alreadyCommitted = 1;

                    /* Special effects from the revealed card. */
                    if (is_special_card(&selectedCard, 10) || is_four_of_a_kind(&wastePile)) {
                        burn_pile(&wastePile);
                        playerData.idiot.burns++;
                        if (is_four_of_a_kind(&wastePile)) playerData.idiot.four_of_a_kind_burns++;
                        continue; /* another turn */
                    }
                    if (is_special_card(&selectedCard, 2)) {
                        continue; /* another turn */
                    }
                    if (is_special_card(&selectedCard, 3)) {
                        Card *mirrored = find_mirrored_card(&wastePile);
                        if (mirrored) {
                            if (is_special_card(topOfWaste, 3)) playerData.idiot.mirror_match++;
                            printf("Mirroring: "); print_card_bracketed(mirrored); printf("\n");
                        } else {
                            printf("Mirroring: [none]\n");
                        }
                    }
                } else {
                    /* Pick up the pile, including the revealed card. */
                    wastePile.pile[wastePile.count++] = selectedCard;
                    handle_pile_pickup(turnPlayer, &wastePile);
                    tricksterWinEligible = 0;
                    continue; /* another turn */
                }
            }

            if (!moveIsValid && !alreadyCommitted) {
                continue; /* illegal choice from hand/face-up → re-prompt */
            }

            /* Hand convenience: dump more of the same rank (if any) */
            if (!alreadyCommitted) {
                /* Commit the chosen card now. */
                wastePile.pile[wastePile.count++] = selectedCard;
                draw_from_pile(turnPlayer, &drawPile);
            }

            /* Offer to dump extras from hand (same rank as selected). */
            int additionalCount = 0;
            for (int i = 0; i < turnPlayer->handCount; ++i)
                if (strcmp(turnPlayer->hand[i].rank, selectedCard.rank) == 0) ++additionalCount;

            if (additionalCount > 0) {
                printf("You have %d additional %s's. Play extra? (0-%d): ",
                       additionalCount, selectedCard.rank, additionalCount);
                int extraChoice = 0; scanf("%d", &extraChoice);
                if (extraChoice > additionalCount) extraChoice = additionalCount;

                for (int k = 0; k < extraChoice; ++k) {
                    for (int j = 0; j < turnPlayer->handCount; ++j) {
                        if (strcmp(turnPlayer->hand[j].rank, selectedCard.rank) == 0) {
                            wastePile.pile[wastePile.count++] = turnPlayer->hand[j];
                            for (int t = j; t < turnPlayer->handCount - 1; ++t) turnPlayer->hand[t] = turnPlayer->hand[t + 1];
                            turnPlayer->handCount--;
                            break;
                        }
                    }
                }
                draw_from_pile(turnPlayer, &drawPile);
                sort_hand_low_to_high(turnPlayer);
            }

            /* Post-commit special handling (if not covered in face-down path). */
            if (!alreadyCommitted) {
                if (is_special_card(&selectedCard, 10) || is_four_of_a_kind(&wastePile)) {
                    burn_pile(&wastePile);
                    continue; /* another turn */
                }
                if (is_special_card(&selectedCard, 2)) {
                    continue; /* another turn */
                }
                if (is_special_card(&selectedCard, 3)) {
                    Card *mirrored = find_mirrored_card(&wastePile);
                    if (mirrored) { printf("Mirroring: "); print_card_bracketed(mirrored); printf("\n"); }
                    else          { printf("Mirroring: [none]\n"); }
                }
            }
        }
        /* ---------------- AI turn ---------------- */
        else {
            ai_play(&aiState, &playerState, &wastePile, &drawPile, difficultyChoice, &aiLastTurnSummary);

            /* Give AI another turn after a burn or a 2 (main loop checks these). */
            if (aiLastTurnSummary.burned)                                 continue;
            if (aiLastTurnSummary.playedCount > 0 &&
                is_special_card(&aiLastTurnSummary.played[0], 2))         continue;
            if (aiLastTurnSummary.playedCount == 0)                       continue; /* picked up */
        }

        /* Win check for the player who just acted (after a successful play). */
        if (turnPlayer->handCount == 0 &&
            turnPlayer->faceUpCount == 0 &&
            turnPlayer->faceDownCount == 0)
        {
            printf("\n%s wins the game!\n", (currentPlayerIndex == 0) ? "Player" : "Opponent");
            if (currentPlayerIndex == 0) {
                unsigned int payoutMultiplier =
                    (difficultyChoice == DIFFICULTY_NORMAL) ? 2 :
                    (difficultyChoice == DIFFICULTY_HARD)   ? 5 : 1;
                playerData.uPlayerMoney += wagerAmount * payoutMultiplier;
                playerData.idiot.wins++;
                playerData.idiot.win_streak++;
                if (playerData.idiot.max_win_streak <= playerData.idiot.win_streak)
                    playerData.idiot.max_win_streak = playerData.idiot.win_streak;
            } else {
                playerData.idiot.losses++;
                playerData.idiot.win_streak = 0;
            }
            break;
        }

        currentPlayerIndex = !currentPlayerIndex;
    }

    if (tricksterWinEligible) playerData.idiot.trickster_wins++;
    checkAchievements();
    save_player_data();
    save_achievements();
}
