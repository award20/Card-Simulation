/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Blackjack card game header.
 * Contains declarations for Blackjack logic and structures.
 * 
 * Pure C implementation.
 */

#ifndef BLACKJACK_H
#define BLACKJACK_H

#include "deck.h"

// Constants for the game
#define MAX_HAND_CARDS 12   // The maximum number of cards a hand can hold

// Structure to represent the player's hand
typedef struct {
    Card cards[MAX_HAND_CARDS];  // Array to hold the cards in the hand
    int count;                   // Number of cards in the hand
    unsigned int bet;            // The bet placed on this hand
    int surrendered;             // Flag indicating if the player surrendered (1 if true)
    int doubled;                 // Flag indicating if the player doubled the bet (1 if true)
    int fromSplit;               // Flag indicating if this hand is from a split (1 if true)
} Hand;

// Function prototypes
static int get_hand_value(Hand *hand);                  // Calculates the value of a hand
static void print_hand(const char *name, Hand *hand);    // Prints the cards in a hand
static void deal_card(Card *deck, int *deckIndex, Hand *hand); // Deals a card to a hand
static int is_pair(Hand *hand);                         // Checks if the hand is a pair (for splitting)
static void play_hand(Card *deck, int *deckIndex, Hand *hand, unsigned long long *uPlayerMoney, int roundNumber); // Plays the hand
static int check_blackjack(Hand *hand);                 // Checks if the hand is a Blackjack

#endif // BLACKJACK_H