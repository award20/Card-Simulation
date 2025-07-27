/*
 * @author: Anthony Ward
 * @upload date: 07/27/2025
 *
 * Selected Games menu file
 */

#include <stdio.h>
#include "deck.h"

// Menu for 21 Blackjack
void blackjack(Card *deck, unsigned long long *uPlayerMoney) {
    int menuSelection;

    while (1) {
        // Display menu for Blackjack options
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Exit\n");

        printf("\nEnter Selection: ");
        scanf("%d", &menuSelection);  // Get user's menu selection

        // Process user selection
        switch(menuSelection){
            case 1:
                blackjack_start(deck, uPlayerMoney);  // Start the Blackjack game
                break;
            case 2:
                blackjackHowToPlay();  // Show instructions for Blackjack
                break;
            case 3:
                gamesMenu(deck);  // Return to the games menu
                break;
            default:
                printf("Please select from the listed options (1-3)\n");  // Handle invalid input
                break;
        }
    }
}

// Menu for Solitaire
void solitaire(Card *deck, unsigned long long *uPlayerMoney) {
    int menuSelection;

    while (1) {
        // Display menu for Solitaire options
        printf("\nPlease select an option:\n");
        printf("1: Play\n");
        printf("2: How to Play\n");
        printf("3: Exit\n");

        printf("\nEnter Selection: ");
        scanf("%d", &menuSelection);    // Get user's menu selection

        // Process user selection
        switch(menuSelection) {
            case 1:
                solitaire_start(deck, uPlayerMoney);    // Start the Solitaire game
                break;
            case 2:
                solitaireHowToPlay();   // Show instructions for Solitaire
                break;
            case 3:
                gamesMenu(deck);    // Return to the games menu
                break;
            default:
                printf("Please select from the listed options (1-3)\n");    // Handle invalid input
                break;
        }
    }
}