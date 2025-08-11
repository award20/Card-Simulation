/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Selected Games menu file
 * 
 * Pure C implementation.
 */

#include <stdio.h>
#include "deck.h"

// Menu for 21 Blackjack
void blackjack(unsigned long long *uPlayerMoney) {
    clear_screen();  // Clear the screen for Blackjack menu
    int menuSelection;

    while (1) {
        // Display menu for Blackjack options
        printf("=== 21 BLACKJACK ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Exit\n");

        printf("\n> ");
        scanf("%d", &menuSelection);  // Get user's menu selection

        // Process user selection
        switch(menuSelection){
            case 1:
                blackjack_start(uPlayerMoney);  // Start the Blackjack game
                break;
            case 2:
                blackjackHowToPlay();  // Show instructions for Blackjack
                break;
            case 3:
                gamesMenu(uPlayerMoney);  // Return to the games menu
                break;
            default:
                printf("Please select from the listed options (1-3)\n");  // Handle invalid input
                break;
        }
    }
}

// Menu for Solitaire
void solitaire(unsigned long long *uPlayerMoney) {
    clear_screen();  // Clear the screen for Solitaire menu
    int menuSelection;

    while (1) {
        // Display menu for Solitaire options
        printf("=== Solitaire ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Exit\n");

        printf("\n> ");
        scanf("%d", &menuSelection);    // Get user's menu selection

        // Process user selection
        switch(menuSelection) {
            case 1:
                solitaire_start(uPlayerMoney);    // Start the Solitaire game
                break;
            case 2:
                solitaireHowToPlay();   // Show instructions for Solitaire
                break;
            case 3:
                gamesMenu(uPlayerMoney);    // Return to the games menu
                break;
            default:
                printf("Please select from the listed options (1-3)\n");    // Handle invalid input
                break;
        }
    }
}

// Menu for Idiot
void idiot(unsigned long long *uPlayerMoney) {
    clear_screen();  // Clear the screen for Idiot menu
    int menuSelection;

    while (1) {
        // Display menu for Idiot options
        printf("=== Idiot ===\n");
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Exit\n");

        printf("\n> ");
        scanf("%d", &menuSelection);    // Get user's menu selection

        // Process user selection
        switch(menuSelection) {
            case 1:
                idiot_start(uPlayerMoney);    // Start the Idiot game
                break;
            case 2:
                idiotHowToPlay();   // Show instructions for Idiot
                break;
            case 3:
                gamesMenu(uPlayerMoney);    // Return to the games menu
                break;
            default:
                printf("Please select from the listed options (1-3)\n");    // Handle invalid input
                break;
        }
    }
}