/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * 21 Blackjack public surface + shared types
 *
 * Exposes the Hand type, a Shoe container for multi-deck play,
 * and the public functions to run Blackjack and show the how-to.
 *
 * Uses config.num_decks (clamped to 1..8) to size the shoe.
 * The shoe is shuffled at init and re-shuffled only when running low.
 */

#ifndef BLACKJACK_H
#define BLACKJACK_H

#include "core.h"

/* ------------------------------------------------------------------------- */
/* Game constants                                                            */
/* ------------------------------------------------------------------------- */

#define MAX_HAND_CARDS            12     /* Max cards a hand can hold.      */
#define MAX_SHOE_DECKS            8      /* Hard cap for config.num_decks.  */
/* Cut-card penetration: reshuffle when >= this % of the shoe has been dealt.
   Typical casinos use ~75–85%. Change this single value to tune. */
#define CUT_CARD_PENETRATION_PERCENT 80

/* ------------------------------------------------------------------------- */
/* Core containers                                                           */
/* ------------------------------------------------------------------------- */

/**
 * Hand
 * One Blackjack hand with its wager and flags.
 */
typedef struct {
    Card          cards[MAX_HAND_CARDS];
    int           count;            /* number of cards in this hand          */
    unsigned int  bet;              /* wager associated with this hand       */
    int           surrendered;      /* 1 if player surrendered this hand     */
    bool          doubled;          /* true if doubled down                  */
    int           fromSplit;        /* 1 if this hand originated from split  */
} Hand;

/**
 * Shoe
 * Multi-deck deal shoe. The next card to deal is at cards[next_index].
 *
 * Invariants:
 *  - 0 <= next_index <= total
 *  - total == decks_in_shoe * DECK_SIZE
 */
typedef struct {
    Card cards[MAX_SHOE_DECKS * DECK_SIZE];
    int  total;        /* total cards in the shoe               */
    int  next_index;   /* next card to deal                     */
    int  decks_in_shoe;/* number of decks used (1..MAX_SHOE_DECKS) */
} Shoe;

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void blackjackHowToPlay(void);
void blackjack_start(void);

/* Round helpers exposed elsewhere in your codebase */
unsigned int get_valid_bet(unsigned int minBet, unsigned int maxBet);
void         handle_insurance(Hand *dealerHand);
void         handle_blackjack(Hand *dealerHand, unsigned int bet);
bool         handle_split(Shoe *shoe, Hand *playerHand1, Hand *playerHand2, unsigned int bet);
void         dealer_play(Shoe *shoe, Hand *dealerHand, int roundNumber);
void         resolve_hands(bool isSplit, Hand *playerHand1, Hand *playerHand2, Hand *dealerHand);
bool         play_again(void);

#endif /* BLACKJACK_H */
