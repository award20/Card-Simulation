/*
 * @author: Anthony Ward
 * @upload date: 07/27/2025
 * 
 * Card processing program that allows for deck creation,
 * shuffling, and printing.
 * 
 * Examples of its use case is found using a simple set of
 * relatively popular card games.
 * 
 * Pure C implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "deck.h"

#define MAX_PLAYER_MONEY 18446744073709551615ULL // Max value for a 64-bit unsigned long long
unsigned long long uPlayerMoney = 0; // Universal Player Money

// Define arrays for the suits and ranks of cards
char *suits[NUM_SUITS] = {"Hearts", "Diamonds", "Clubs", "Spades"};
char *ranks[NUM_RANKS] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace"};

// Function to initialize the deck with cards
void initialize_deck(Card *deck) {
    int i = 0;
    for (int s = 0; s < NUM_SUITS; s++) {           // Loop through each suit
        for (int r = 0; r < NUM_RANKS; r++) {       // Loop through each rank
            deck[i].suit = suits[s];                 // Assign suit to card
            deck[i].rank = ranks[r];                 // Assign rank to card
            i++;
        }
    }
}

// Function to shuffle the deck
void shuffle_deck(Card *deck) {
    for (int i = DECK_SIZE - 1; i > 0; i--) {
        int j = rand() % (i + 1);                  // Random index for swapping
        Card temp = deck[i];                        // Swap cards
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

// Function to print all cards in the deck
void print_deck(Card *deck) {
    for (int i = 0; i < DECK_SIZE; i++) {         // Loop through all cards
        printf("%s of %s\n", deck[i].rank, deck[i].suit); // Print the rank and suit
    }
    fflush(stdout);   // Ensure output is printed immediately
}

// Function to clear the terminal screen
void clear_screen() {
#ifdef _WIN32
    system("cls");    // Windows OS command to clear screen
#else
    system("clear");  // Unix-based systems (Linux/Mac) command to clear screen
#endif
}

// Main menu with player selection
void deckMenu(Card *deck);

// Game selection menu
void gamesMenu(Card *deck);

// Main function
int main() {
    Card deck[DECK_SIZE];      // Array to hold deck of cards
    srand(time(NULL));         // Initialize random number generator for shuffle
    initialize_deck(deck);     // Initialize the deck with cards

    do {
        clear_screen();        // Clear the screen at the beginning of each round

        printf("Welcome to a Playing Card Simulation!\n");
        printf("Enter your starting money amount: $");
        scanf("%lld", &uPlayerMoney);  // Get the player's starting money

        // Check if entered money is valid
        if (uPlayerMoney < 10) {
            printf("You inputted $%lld\n", uPlayerMoney);
            printf("You need at least $10 to play. Press Enter to continue.");
            getchar(); getchar();
        }
        else if (uPlayerMoney > MAX_PLAYER_MONEY) {
            printf("You cannot enter more than $%lld. Press Enter to continue.", MAX_PLAYER_MONEY);
        }
    } while (uPlayerMoney < 10 || uPlayerMoney > MAX_PLAYER_MONEY); // Keep asking until valid amount is entered

    deckMenu(deck);  // Call the menu for deck operations
    return 0;
}

// Main menu to select deck actions
void deckMenu(Card *deck) {
    int menuSelection;

    while (1) {
        clear_screen();   // Clear the screen for menu
        printf("=== MAIN MENU ===\n");
        printf("Funds: $%lld\n", uPlayerMoney);   // Display player's current money
        printf("\nPlease select from the options below:\n");
        printf("1: Initialize New Deck\n");
        printf("2: Shuffle Deck\n");
        printf("3: Print Deck\n");
        printf("4: Card Games Selection\n");
        printf("5: Change Funds\n");
        printf("6: Exit\n");

        printf("\nEnter Selection: ");
        scanf("%d", &menuSelection);   // Get user's menu choice

        // Process the user's selection
        switch(menuSelection) {
            case 1:
                printf("\nInitializing a new deck.\n");
                initialize_deck(deck);  // Initialize a new deck
                break;
            case 2:
                printf("\nShuffling the deck.\n");
                shuffle_deck(deck);  // Shuffle the deck
                break;
            case 3:
                printf("\nPrinting the deck.\n");
                print_deck(deck);  // Print all cards in the deck
                printf("\nPress enter to continue...");
                getchar(); getchar();  // Wait for user input to continue
                break;
            case 4:
                gamesMenu(deck);  // Open games menu for card games
                break;
            case 5:
                do {
                    printf("\nPlease enter the desired funds: $");
                    scanf("%lld", &uPlayerMoney);   // Set the player's funds
                    if (uPlayerMoney < 10) {
                        printf("You need at least $10 to play. Press Enter to continue.");
                        getchar(); getchar();
                    }
                    else if (uPlayerMoney > MAX_PLAYER_MONEY) {
                        printf("You cannot enter more than $%lld. Press Enter to continue.", MAX_PLAYER_MONEY);
                    }
                } while (uPlayerMoney < 10 || uPlayerMoney > MAX_PLAYER_MONEY); // Ensure valid money amount
                printf("\n");
                break;
            case 6:
                printf("\nExiting.\n");
                exit(0);  // Exit the program
            default:
                printf("\nPlease select a valid option (1-6)\n");  // Handle invalid input
                break;
        }
    }
}

// Game selection menu with options for various card games
void gamesMenu(Card *deck) {
    int menuSelection;

    while (1) {
        clear_screen();  // Clear the screen for the game menu
        printf("=== GAME MENU ===\n");
        printf("Funds: $%lld\n", uPlayerMoney);  // Display player's current funds
        printf("\nSelect a game:\n");
        printf("1: 21 Blackjack\n");
        printf("2: Texas Hold'em\n");
        printf("3: 5-Card Poker\n");
        printf("4: Solitaire\n");
        printf("5: Rummy\n");
        printf("6: Idiot\n");
        printf("7: Return to Main Menu\n");

        printf("\nEnter Selection: ");
        scanf("%d", &menuSelection);  // Get user's game selection

        // Handle the game selection logic
        switch(menuSelection) {
            case 1:
                clear_screen();
                printf("=== 21 Blackjack ===\n");
                blackjack(deck, &uPlayerMoney);  // Launch Blackjack game
                break;
            case 2:
                clear_screen();
                printf("=== Texas Hold'em ===\n");
                printf("\nGame is currently in development. Press Enter to continue.");
                getchar(); getchar();
                break;
            case 3:
                clear_screen();
                printf("=== 5-Card Poker ===\n");
                printf("\nGame is currently in development. Press Enter to continue.");
                getchar(); getchar();
                break;
            case 4:
                clear_screen();
                printf("=== Solitaire ===\n");
                solitaire(deck, &uPlayerMoney); // Launch Solitaire game
                break;
            case 5:
                clear_screen();
                printf("=== Rummy ===\n");
                printf("\nGame is currently in development. Press Enter to continue.");
                getchar(); getchar();
                break;
            case 6:
                clear_screen();
                printf("=== Idiot===\n");
                printf("\nGame is currently in development. Press Enter to continue.");
                getchar(); getchar();
                break;
            case 7:
                deckMenu(deck);  // Return to deck menu
                break;
            default:
                printf("\nPlease select a valid option (1-7)\n");  // Handle invalid input
                break;
        }
    }
}
