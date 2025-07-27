/*
 * @author: Anthony Ward
 * @upload date: 07/26/2025
 *
 * Selected Games menu file
 */

#include <stdio.h>
#include "deck.h"

// Menu for 21 Blackjack
void blackjack(Card *deck, unsigned long long *uPlayerMoney) {
    int menuSelection;
    int exitMenu = 3;  // Exit option from Blackjack menu

    do {
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
                printf("Please select from the listed options (1-%d)\n", exitMenu);  // Handle invalid input
                break;
        }
    } while (menuSelection != exitMenu);  // Continue until exit option is selected
}
