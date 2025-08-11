/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Idiot card game header.
 * Contains declarations for the Idiot game logic and structures.
 * 
 * Pure C implementation.
 */

#ifndef IDIOT_H
#define IDIOT_H

#include "deck.h"

#define HAND_SIZE 3 // Number of hand cards
#define FACE_UP_SIZE 3  // Number of face-up cards
#define FACE_DOWN_SIZE 3    // Number of face-down cards
#define MAX_PILE 52 // Max cards in a pile
#define MAX_HAND_CARDS 52  // Allow the player to hold the entire deck of 52 cards

// Add this struct for the player
typedef struct {
    Card hand[MAX_HAND_CARDS];  // Max hand size to accommodate entire deck
    Card faceUp[FACE_UP_SIZE];
    Card faceDown[FACE_DOWN_SIZE];
    int handCount;
    int faceUpCount;
    int faceDownCount;
} IdiotPlayer;

// Add this struct for the card pile
typedef struct {
    Card pile[MAX_PILE];
    int count;
} CardPile;

// Add this struct for AI's last move
typedef struct {
    int playedCount; // 1 or 2
    Card played[2];
    int burned;
    int mirrored; // 1 if last was a 3
    Card *mirroredCard;
} AILastMove;

// Add this struct for player's last move
typedef struct {
    int playedCount;
    Card played[MAX_HAND_CARDS];
} PlayerLastMove;

// Function prototypes
static int card_value(const Card *card);
static void display_idiot_game(IdiotPlayer *player, IdiotPlayer *opponent, CardPile *drawPile, CardPile *wastePile, AILastMove *lastMove);
static int can_play_card(const Card *top, const Card *next, const CardPile *wastePile);
static int is_four_of_a_kind(CardPile *pile);
static int is_special_card(const Card *card, int value);
static void burn_pile(CardPile *pile);
static void draw_from_pile(IdiotPlayer *p, CardPile *drawPile);
static void swap_hand_cards(IdiotPlayer *p);
static void sort_hand_low_to_high(IdiotPlayer *p);
static void ai_play(IdiotPlayer *ai, CardPile *pile, CardPile *drawPile, int difficulty, AILastMove *lastMove);
static Card* find_mirrored_card(CardPile *pile);
static void handle_pile_pickup(IdiotPlayer *p, CardPile *pile);


#endif