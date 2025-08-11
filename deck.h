/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Deck of Cards header file with function prototypes
 * 
 * Pure C implementation.
 */

#ifndef deck_h
#define deck_h

#define NUM_SUITS 4       // Total number of suits in the deck
#define NUM_RANKS 13      // Total number of ranks in the deck
#define DECK_SIZE 52      // Total number of cards in the deck

// Define the structure for a card
typedef struct {
    char *suit;          // Suit of the card (e.g., Hearts, Diamonds)
    char *rank;          // Rank of the card (e.g., 2, Ace, King)
    int revealed;        // Flag to indicate if the card is revealed or not
} Card;

// Function prototypes for card deck operations

// Functions in main.c
void clear_screen();      // Clear terminal screen
void initialize_deck(Card *deck);    // Initialize a deck of cards
void shuffle_deck(Card *deck);       // Shuffle the deck
void print_deck(Card *deck);         // Print all cards in the deck
void deckMenu(unsigned long long *uPlayerMoney);           // Main menu for deck operations
void gamesMenu(unsigned long long *uPlayerMoney);          // Menu for game selection

// Functions in games.c
void blackjack(unsigned long long *uPlayerMoney);   // Menu for Blackjack game
void solitaire(unsigned long long *uPlayerMoney);   // Menu for Solitaire game
void idiot(unsigned long long *uPlayerMoney);   // Menu for Idiot game

// Functions in blackjack.c
void blackjackHowToPlay();   // Instructions for playing Blackjack
void blackjack_start(unsigned long long *uPlayerMoney);  // Start the Blackjack game

// solitaire.c
void solitaireHowToPlay();  // Instructions for playing Solitaire
void solitaire_start(unsigned long long *uPlayerMoney); // Start the Solitaire game

// solitaire.c
void idiotHowToPlay();  // Instructions for playing Idiot
void idiot_start(unsigned long long *uPlayerMoney); // Start the Idiot game

#endif