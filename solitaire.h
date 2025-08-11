/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Solitaire header file with function prototypes
 * 
 * Pure C implementation.
 */

#ifndef SOLITAIRE_H
#define SOLITAIRE_H

#include "deck.h"

// Game constants
#define COLUMNS 7           // The number of columns for cards to be placed
#define FOUNDATION_PILES 4  // The number of foundation piles that hold cards in ascending order
#define MAX_DRAW_STACK 52   // The maximum number of cards that can exist in the draw pile
#define MAX_FOUNDATION 13   // The maximum number of cards that can exist in the foundation piles
#define GAME_SAVE "solitaire_save.dat" // File to save the game state

// Define a Stack structure to hold cards
typedef struct {
    Card cards[MAX_DRAW_STACK];
    int count;
} Stack;

// Define the main game structure (KlondikeGame)
typedef struct {
    Card table[COLUMNS][MAX_DRAW_STACK];     // 2D array for table (columns of cards)
    int table_counts[COLUMNS];               // Array to store the number of cards in each column
    Stack drawPile;                          // Stack for the remaining cards (draw pile)
    Stack wastePile;                         // Stack for the discarded cards (waste pile)
    Stack foundation[FOUNDATION_PILES];      // Array of foundation piles (4 suits)
    int difficulty;                          // Game difficulty (1: Easy, 2: Normal, 3: Hard)
} KlondikeGame;

// Function prototypes
void solitaire_start(unsigned long long *uPlayerMoney); // Starts the Solitaire game
void solitaireHowToPlay();  // Displays the how-to-play instructions for the user
static void setup_klondike(KlondikeGame *game, Card *deck); // Initializes the game setup (table, foundation, draw pile, etc.)
static void display_game(KlondikeGame *game);   // Displays the current state of the game
static void draw_card(KlondikeGame *game);  // Draws cards from the draw pile to the waste pile
static bool play_klondike(KlondikeGame *game, unsigned long long *uPlayerMoney, unsigned int bet);  // The main game loop where the player interacts with the game
static int can_move_to_foundation(Card card, Stack *foundation);    // Checks if a card can be moved to a foundation pile
static int can_move_to_table(Card card, Card dest); // Check if a card can be moved to a table column
static int is_red(Card card);   // Checks if a card is red (Hearts or Diamonds)
static void create_undo(KlondikeGame *game);    // Saves the curren state of the game to allow undo functionality
static void undo_move(KlondikeGame *game);  // Undoes the last move and restores the game state
static void auto_complete(KlondikeGame *game);  // Automatically completes the game if possible
static int can_auto_complete(KlondikeGame *game);  // Checks if the game can be auto-completed

#endif // SOLITAIRE_H
