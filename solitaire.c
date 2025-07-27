/*
 * @author: Anthony Ward
 * @upload date: 07/27/2025
 *
 * Klondike Solitaire game implementation.
 *
 * This file contains the game logic for Klondike Solitaire,
 * including the setup, rules, and actions for playing the game.
 * It allows users to interact with the tableau, draw pile, waste pile,
 * and foundation piles in an attempt to move all cards to the foundations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "deck.h"

#define COLUMNS 7           // The number of columns for cards to be placed
#define FOUNDATION_PILES 4  // The number of foundation piles that hold cards in ascending order
#define MAX_DRAW_STACK 52   // The maximum number of cards that can exist in the draw pile
#define MAX_FOUNDATION 13   // The maximum number of cards that can exist in the foundation piles

// Define a Stack structure to hold cards
typedef struct {
    Card cards[MAX_DRAW_STACK];
    int count;
} Stack;

// Define the main game structure (KlondikeGame)
typedef struct {
    Card table[COLUMNS][MAX_DRAW_STACK];     // 2D array for tableau (columns of cards)
    int table_counts[COLUMNS];               // Array to store the number of cards in each column
    Stack drawPile;                          // Stack for the remaining cards (draw pile)
    Stack wastePile;                         // Stack for the discarded cards (waste pile)
    Stack foundation[FOUNDATION_PILES];      // Array of foundation piles (4 suits)
    int difficulty;                          // Game difficulty (1: Easy, 2: Normal, 3: Hard)
} KlondikeGame;

// Save/undo buffer and flag for undo functionality
KlondikeGame undoBuffer;
int hasUndo = 0;

void solitaire_start(Card *deck, unsigned long long *uPlayerMoney); // Starts the Solitaire game
void solitaireHowToPlay();  // Displays the how-to-play instructions for the user
static void setup_klondike(KlondikeGame *game, Card *deck); // Initializes the game setup (table, foundation, draw pile, etc.)
static void display_game(KlondikeGame *game);   // Displays the current state of the game
static void draw_card(KlondikeGame *game);  // Draws cards from the draw pile to the waste pile
static bool play_klondike(KlondikeGame *game);  // The main game loop where the player interacts with the game
static int can_move_to_foundation(Card card, Stack *foundation);    // Checks if a card can be moved to a foundation pile
static int can_move_to_table(Card card, Card dest); // Check if a card can be moved to a table column
static int is_red(Card card);   // Checks if a card is red (Hearts or Diamonds)
static void create_undo(KlondikeGame *game);    // Saves the curren state of the game to allow undo functionality
static void undo_move(KlondikeGame *game);  // Undoes the last move and restores the game state

// Function to create an undo buffer of the current game state
void create_undo(KlondikeGame *game) {
    memcpy(&undoBuffer, game, sizeof(KlondikeGame));  // Copy the game state into undoBuffer
    hasUndo = 1;  // Set the undo flag to true
}

// Function to undo the last move
void undo_move(KlondikeGame *game) {
    if (hasUndo) {
        memcpy(game, &undoBuffer, sizeof(KlondikeGame));  // Restore the game state from undoBuffer
        hasUndo = 0;  // Reset the undo flag
        printf("\nUndo performed.\n");
    }
    else {
        printf("\nNo undo available.\n");  // Notify the user if no undo is available
    }
}

//////////////////////////////
// How to play instructions //
//////////////////////////////

// Function that displays the "How to Play" instructions for Solitaire
void solitaireHowToPlay() {
    clear_screen();
    printf("=== HOW TO PLAY: KLONDIKE SOLITAIRE ===\n\n");

    // Objective section
    printf("--- Objective ---\n");
    printf("The goal of Klondike Solitaire is to move all the cards to the foundation piles.\n");
    printf("Cards must be sorted by suit in ascending order, from Ace to King.\n\n");

    // Setup section
    printf("--- Setup ---\n");
    printf("1. Seven tableau columns with cards laid face down, with only the top card face up.\n");
    printf("2. A draw pile containing the remaining cards.\n");
    printf("3. Four foundation piles for each suit.\n\n");

    // Controls section
    printf("--- Controls ---\n");
    printf("1. Move cards between tableau columns and foundation piles.\n");
    printf("2. Only King can be moved to an empty tableau column.\n");
    printf("3. Cards must follow alternating colors in descending order in tableau columns.\n");
    printf("4. Cards can be drawn from the draw pile to the waste pile.\n\n");

    // Difficulty section
    printf("--- Difficulty Levels ---\n");
    printf("1. Easy: Draw 1 card at a time, with recycling draw pile.\n");
    printf("2. Normal: Draw 1 card at a time, no recycling.\n");
    printf("3. Hard: Draw 3 cards at a time, no recycling.\n\n");

    // Winning section
    printf("--- Winning ---\n");
    printf("You win when all cards are moved to the foundation piles in the correct order.\n");

    printf("\nPress Enter to return...\n");
    getchar(); getchar();
    clear_screen();
}
//////////////////////////
// Solitaire Game Logic //
//////////////////////////

// Function that starts the game and manages gameplay flow
void solitaire_start(Card *deck, unsigned long long *uPlayerMoney) {
    KlondikeGame game;
    srand(time(NULL));  // Initialize random seed

    goto fresh_game;  // Jump to the fresh game initialization section

fresh_game:
    // Game initialization for a fresh start
    printf("\n=== Select Difficulty ===\n");
    printf("1: Easy\n");
    printf("2: Normal\n");
    printf("3: Hard\n");
    printf("Enter choice: ");
    scanf("%d", &game.difficulty);  // Get difficulty choice from the player

    unsigned int bet = 0;
    unsigned int minBet = 10;
    unsigned int maxBet = 100;

    // Bet selection for non-Easy modes
    if (game.difficulty == 2 || game.difficulty == 3) {
        do {
            if (*uPlayerMoney > maxBet) 
                printf("Enter your bet ($%d - $%d): ", minBet, maxBet);
            else 
                printf("Enter your bet ($%d - $%lld): ", minBet, *uPlayerMoney);
            scanf("%d", &bet);  // Get bet amount from the player
        } while (bet < minBet || bet > maxBet || bet > *uPlayerMoney);  // Validate bet
        *uPlayerMoney -= bet;  // Deduct the bet from player's money
    }

    // Initialize and shuffle deck for a fresh game
    initialize_deck(deck);
    for (int i = 0; i < DECK_SIZE; i++) {
        deck[i].revealed = 0;  // Set all cards as face down initially
    }
    shuffle_deck(deck);  // Shuffle the deck of cards

    // Setup the game (columns, draw pile, waste pile, foundation)
    setup_klondike(&game, deck);

    // Main gameplay loop
    int won = play_klondike(&game);  // Call the game loop to play the solitaire game

    // Game outcome handling
    if (won) {
        clear_screen();
        if (game.difficulty == 2) {
            *uPlayerMoney += bet * 2;
            printf("You win! Earned 2x your bet: $%d\n", bet * 2);
        }
        else if (game.difficulty == 3) {
            *uPlayerMoney += bet * 5;
            printf("You win! Earned 5x your bet: $%d\n", bet * 5);
        }
        else {
            printf("You won (Easy Mode).\n");
        }
    }
    else {
        printf("\nGame over. You did not complete all foundations.\n");
    }

    printf("Final Balance: $%lld\n", *uPlayerMoney);
    printf("\nPress Enter to return...");
    getchar(); getchar();
    clear_screen();
}

// Function to initialize the game setup (table, foundation, etc.)
static void setup_klondike(KlondikeGame *game, Card *deck) {
    int deckIndex = 0;
    for (int i = 0; i < COLUMNS; i++) {
        for (int j = 0; j <= i; j++) {
            game->table[i][j] = deck[deckIndex++];  // Place cards in the tableau columns
            game->table[i][j].revealed = (j == i);  // Only the top card is revealed
        }
        game->table_counts[i] = i + 1;  // Store the number of cards in each column
    }

    // Initialize the draw pile (remaining cards)
    game->drawPile.count = 0;
    for (; deckIndex < DECK_SIZE; deckIndex++) {
        deck[deckIndex].revealed = 1;  // Draw pile cards are face up when drawn
        game->drawPile.cards[game->drawPile.count++] = deck[deckIndex];
    }

    // Initialize foundation piles (empty at the start)
    for (int i = 0; i < FOUNDATION_PILES; i++) {
        game->foundation[i].count = 0;
    }

    game->wastePile.count = 0;  // Waste pile starts empty
}

// Function to display the current game state (table, piles, etc.)
static void display_game(KlondikeGame *game) {
    clear_screen();
    printf("--- Game View ---\n");
    printf("Draw Pile: %d cards\n", game->drawPile.count);
    if (game->wastePile.count > 0) {
        Card top = game->wastePile.cards[game->wastePile.count - 1];
        printf("Top of Waste: [%s of %s]\n", top.rank, top.suit);
    } 
    else {
        printf("Waste Pile: empty\n");
    }

    // Display the foundation piles
    for (int i = 0; i < FOUNDATION_PILES; i++) {
        if (game->foundation[i].count > 0) {
            Card top = game->foundation[i].cards[game->foundation[i].count - 1];
            printf("Foundation %d: [%s of %s]\n", i + 1, top.rank, top.suit);
        }
        else {
            printf("Foundation %d: empty\n", i + 1);
        }
    }

    // Display the tableau columns
    for (int i = 0; i < COLUMNS; i++) {
        printf("Column %d: ", i + 1);
        for (int j = 0; j < game->table_counts[i]; j++) {
            if (!game->table[i][j].revealed) {
                printf("[???] ");  // Face-down cards
            } 
            else {
                printf("[%s of %s] ", game->table[i][j].rank, game->table[i][j].suit);  // Face-up cards
            }
        }
        printf("\n");
    }
}

// Function to draw a card from the draw pile to the waste pile
static void draw_card(KlondikeGame *game) {
    int drawCount = (game->difficulty == 3) ? 3 : 1;  // Determine how many cards to draw (based on difficulty)
    int actualDraw = (game->drawPile.count < drawCount) ? game->drawPile.count : drawCount;

    if (actualDraw > 0) {
        for (int i = 0; i < actualDraw; i++) {
            game->wastePile.cards[game->wastePile.count++] = game->drawPile.cards[--game->drawPile.count];  // Move card to waste pile
        }
    } 
    else if (game->difficulty == 1 && game->wastePile.count > 0) {
        // Recycle waste into draw pile if on Easy mode
        for (int i = game->wastePile.count - 1; i >= 0; i--) {
            game->drawPile.cards[game->drawPile.count++] = game->wastePile.cards[i];
        }
        game->wastePile.count = 0;  // Reset the waste pile
        draw_card(game);  // Draw again after recycling
    }
}

// Function to check if the card is red (Hearts or Diamonds)
static int is_red(Card card) {
    // Check if the card's suit is either Hearts or Diamonds
    return strcmp(card.suit, "Hearts") == 0 || strcmp(card.suit, "Diamonds") == 0;
}

// Function to check if a card can be moved to a foundation pile
static int can_move_to_foundation(Card card, Stack *foundation) {
    // Convert the card's rank to a numerical value
    int cardVal = atoi(card.rank);  
    // Handle special cases for face cards
    if (strcmp(card.rank, "Ace") == 0) cardVal = 1;
    else if (strcmp(card.rank, "Jack") == 0) cardVal = 11;
    else if (strcmp(card.rank, "Queen") == 0) cardVal = 12;
    else if (strcmp(card.rank, "King") == 0) cardVal = 13;

    // If foundation is empty, only Ace can be placed
    if (foundation->count == 0) return cardVal == 1;

    // Check the top card of the foundation pile
    Card top = foundation->cards[foundation->count - 1];
    int topVal = atoi(top.rank);
    if (strcmp(top.rank, "Ace") == 0) topVal = 1;
    else if (strcmp(top.rank, "Jack") == 0) topVal = 11;
    else if (strcmp(top.rank, "Queen") == 0) topVal = 12;
    else if (strcmp(top.rank, "King") == 0) topVal = 13;

    // Check if the card's suit matches the top card and the rank is one greater
    return strcmp(card.suit, top.suit) == 0 && cardVal == topVal + 1;
}

// Function to return the numerical value of a card (Ace = 1, Jack = 11, etc.)
static int card_value(Card card) {
    if (strcmp(card.rank, "Ace") == 0) return 1;  // Ace is 1
    if (strcmp(card.rank, "Jack") == 0) return 11; // Jack is 11
    if (strcmp(card.rank, "Queen") == 0) return 12; // Queen is 12
    if (strcmp(card.rank, "King") == 0) return 13; // King is 13
    return atoi(card.rank);  // Return the integer value of the card
}

// Function to check if a card can be moved to a tableau column
static int can_move_to_table(Card card, Card dest) {
    // Compare the card's value and the destination card's value
    int cardVal = card_value(card);
    int destVal = card_value(dest);
    // Ensure alternating colors and card ranks are descending (cardVal should be 1 less than destVal)
    return is_red(card) != is_red(dest) && cardVal == destVal - 1;
}

/* 
 * Function to handle moving cards between piles in the game.
 * This function allows the user to move cards between different piles: 
 * waste to foundation, waste to columns, columns to columns, etc.
 */
static void move_card(KlondikeGame *game) {
    int choice;  // Variable to hold user's choice for moving cards

    // Prompt the user to choose which type of move to make
    printf("\n1: Waste to foundation\n2: Waste to column\n3: Column to column\n4: Column to foundation\n5: Foundation to column\n6: Cancel\nChoice: ");
    scanf("%d", &choice);  // Get the user's choice

    // Create an undo buffer before making any changes
    create_undo(game);

    // Handle waste to foundation move (moving top card from waste pile to foundation)
    if (choice == 1 && game->wastePile.count > 0) {
        Card card = game->wastePile.cards[game->wastePile.count - 1]; // Get the top card of the waste pile
        // Try moving the card to each foundation pile
        for (int i = 0; i < FOUNDATION_PILES; i++) {
            if (can_move_to_foundation(card, &game->foundation[i])) {
                // If valid, add the card to the foundation pile
                game->foundation[i].cards[game->foundation[i].count++] = card;
                game->wastePile.count--;  // Remove the card from the waste pile
                return;  // Exit after successfully moving the card
            }
        }
    }
    // Handle waste to column move (moving top card from waste pile to tableau column)
    else if (choice == 2 && game->wastePile.count > 0) {
        Card card = game->wastePile.cards[game->wastePile.count - 1]; // Get the top card of the waste pile
        int dest;  // Destination column for the move
        printf("To column #: ");
        scanf("%d", &dest);  // Get the column number from the user
        dest--;  // Adjust for zero-based indexing
        // Check if the column is valid
        if (dest >= 0 && dest < COLUMNS) {
            // Handle empty column case (only a King can be moved to an empty column)
            if (game->table_counts[dest] == 0 && strcmp(card.rank, "King") == 0) {
                game->table[dest][0] = card;
                game->table[dest][0].revealed = 1;  // Reveal the card
                game->table_counts[dest] = 1;  // Update the column count
                game->wastePile.count--;  // Remove the card from the waste pile
            }
            // Handle non-empty column (ensure valid descending sequence)
            else if (game->table_counts[dest] > 0 && can_move_to_table(card, game->table[dest][game->table_counts[dest] - 1])) {
                game->table[dest][game->table_counts[dest]++] = card;  // Add card to column
                game->wastePile.count--;  // Remove card from waste pile
            }
        } 
    }
    // Handle column to column move (moving card from one tableau column to another)
    else if (choice == 3) {
        int from, to;
        printf("From column #: ");
        scanf("%d", &from);  // Get the source column
        printf("To column #: ");
        scanf("%d", &to);  // Get the destination column
        from--; to--;  // Adjust for zero-based indexing
        // Check if the columns are valid
        if (from >= 0 && from < COLUMNS && to >= 0 && to < COLUMNS && game->table_counts[from] > 0) {
            int start = -1;
            // Find the first revealed card in the source column
            for (int i = 0; i < game->table_counts[from]; i++) {
                if (game->table[from][i].revealed) {
                    start = i;
                    break;
                }
            }
            // If a revealed card is found, check if it can be moved to the destination column
            if (start != -1) {
                Card *dest = game->table_counts[to] > 0 ? &game->table[to][game->table_counts[to] - 1] : NULL;
                for (int i = start; i < game->table_counts[from]; i++) {
                    // Check if the move is valid
                    if ((dest == NULL && strcmp(game->table[from][i].rank, "King") == 0) ||
                        (dest != NULL && can_move_to_table(game->table[from][i], *dest))) {
                        // Move the sequence of cards to the destination column
                        int moveCount = game->table_counts[from] - i;
                        for (int j = 0; j < moveCount; j++) {
                            game->table[to][game->table_counts[to]++] = game->table[from][i + j];
                        }
                        game->table_counts[from] = i;
                        // If there are remaining cards in the column, reveal the next one
                        if (i > 0) game->table[from][i - 1].revealed = 1;
                        return;  // Exit after successfully moving the cards
                    }
                }
                printf("\nInvalid move: No valid sequence to move.\n");
            }
        }
    }
    // Handle column to foundation move (moving top card from a column to a foundation pile)
    else if (choice == 4) {
        int col;
        printf("From column #: ");
        scanf("%d", &col);  // Get the source column
        col--;  // Adjust for zero-based indexing
        // Check if the column is valid
        if (col >= 0 && col < COLUMNS && game->table_counts[col] > 0 &&
            game->table[col][game->table_counts[col] - 1].revealed) {
            Card card = game->table[col][game->table_counts[col] - 1];  // Get the top card from the column
            // Try to move the card to each foundation pile
            for (int i = 0; i < FOUNDATION_PILES; i++) {
                if (can_move_to_foundation(card, &game->foundation[i])) {
                    game->foundation[i].cards[game->foundation[i].count++] = card;  // Add card to foundation
                    game->table_counts[col]--;  // Remove card from the column
                    if (game->table_counts[col] > 0)
                        game->table[col][game->table_counts[col] - 1].revealed = 1;  // Reveal the next card
                    break;  // Exit after moving the card
                }
            }
        }
    }
    // Handle foundation to column move (moving top card from foundation to a tableau column)
    else if (choice == 5) {
        int from, to;
        printf("From foundation #: ");
        scanf("%d", &from);  // Get the source foundation pile
        printf("To column #: ");
        scanf("%d", &to);  // Get the destination column
        from--; to--;  // Adjust for zero-based indexing
        // Check if the foundation and column are valid
        if (from >= 0 && from < FOUNDATION_PILES && game->foundation[from].count > 0 &&
            to >= 0 && to < COLUMNS) {
            Card card = game->foundation[from].cards[game->foundation[from].count - 1];  // Get the top card from the foundation
            // Check if the card can be moved to the destination column
            if ((game->table_counts[to] == 0 && strcmp(card.rank, "King") == 0) ||
                (game->table_counts[to] > 0 && can_move_to_table(card, game->table[to][game->table_counts[to] - 1]))) {
                card.revealed = 1;  // Reveal the card
                game->table[to][game->table_counts[to]++] = card;  // Move the card to the tableau column
                game->foundation[from].count--;  // Remove the card from the foundation
            }
        }
    }
    else {
        printf("\nMove canceled.\n");  // Handle invalid choice
    }
}

// Function that handles the main game loop for Solitaire
static bool play_klondike(KlondikeGame *game) {
    int action;
    while (1) {
        display_game(game);  // Display the current game state
        printf("\nOptions:\n");
        printf("1: Draw card\n");
        printf("2: Move card\n");
        if (game->difficulty == 1) {
            printf("3: Undo move\n");
            printf("4: Quit game\n");
        }
        else {
            printf("3: Quit game\n");
        }
        printf("Enter action: ");
        scanf("%d", &action);  // Get the user's action

        if (action == 1) {
            create_undo(game);  // Save the current game state before drawing
            draw_card(game);  // Draw a card
        }
        else if (action == 2) {
            move_card(game);  // Move a card
        }
        else if (action == 3 && game->difficulty == 1) {
            undo_move(game);  // Undo the last move (if available)
        } 

        if ((game->difficulty == 1 && action == 4) || (game->difficulty != 1 && action == 3)) {
            int complete = 0;
            // Check if all foundation piles are complete (if so, the player wins)
            for (int i = 0; i < FOUNDATION_PILES; i++) {
                if (game->foundation[i].count == MAX_FOUNDATION) complete++;
            }
            if (complete == FOUNDATION_PILES) return true;  // If all foundations are complete, return true (game won)
        }
    return false;  // If the game isn't complete yet, continue playing
    }
}