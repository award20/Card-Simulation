/*
 * @author: Anthony Ward
 * @upload date: 07/26/2025
 *
 * 21 Blackjack implementation using card logic
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include "deck.h"

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

// Function declarations for internal functions (not accessible outside this file)
static int get_hand_value(Hand *hand);                  // Calculates the value of a hand
static void print_hand(const char *name, Hand *hand);    // Prints the cards in a hand
static void deal_card(Card *deck, int *deckIndex, Hand *hand); // Deals a card to a hand
static int is_pair(Hand *hand);                         // Checks if the hand is a pair (for splitting)
static void play_hand(Card *deck, int *deckIndex, Hand *hand, unsigned long long *uPlayerMoney, int roundNumber); // Plays the hand
static int check_blackjack(Hand *hand);                 // Checks if the hand is a Blackjack

//////////////////////////////
// How to play instructions //
//////////////////////////////

// Function to display how to play instructions for Blackjack
void blackjackHowToPlay() {
    clear_screen();   // Clear the screen before showing instructions
    printf("=== HOW TO PLAY: 21 BLACKJACK ===\n");

    // Game Objective
    printf("\nObjective: Get a hand total as close to 21 without going over. Beat the dealer's hand to win.\n");

    // Card Values
    printf("Card Values:\n");
    printf(" - Number Cards (2-10) = Cards face value\n");
    printf(" - Face Cards (Jack, Queen, King) = 10 points\n");
    printf(" - Ace = 1 or 11 points -- whichever is more favorable to the hand\n");

    // Gameplay Overview
    printf("\n---Gameplay Overview---\n");
    printf("Initial Deal:\n");
    printf(" - Each player (and the dealer) is dealt 2 cards.\n");
    printf(" - Players' cards are face up\n");
    printf(" - The dealer has one card face up (the \"upcard\"), one face down (the \"hole\" card)\n");

    // Player Actions
    printf("Player Action:\n");
    printf(" - Hit = Take another card\n");
    printf(" - Stand = Stop taking cards\n");
    printf(" - Double Down = Double your bet, take exactly one more card, and stand\n");
    printf(" - Split = If your two cards are a pair (e.g., 7-7), you can split into two hands (requires an extra bet)\n");
    printf(" - Surrender (Optional) = Forfeit your hand early, get half of your bet back\n");
    printf("    - Only allowed on the first two cards\n");

    // Dealer's Turn
    printf("Dealer's Turn:\n");
    printf(" - Dealer only plays once all players finish making their decisions\n");
    printf(" - Dealer reveals the hole card\n");
    printf("    - On a soft 17 (Ace-7) the dealer will continue to hit\n");
    printf(" - Dealer stops hitting on a 17\n");
    printf(" - If the dealer busts (>21) all remaining players win\n");

    // Hand Comparison
    printf("Compare Hands:\n");
    printf(" - Closer to 21 wins\n");
    printf(" - Tie = Push -> your bet is returned\n");
    printf(" - Blackjack (Ace + 10 value card on first two cards) beats 21 made in other ways\n");
    printf("    - Blackjack combinations: Ace-10, Ace-Jack, Ace-Queen, Ace-King\n");

    // Payouts
    printf("\n---Payouts---\n");
    printf("\nWin (normal)        = 1:1\n");
    printf("Blackjack (natural) = 3:2\n");
    printf("Insurance win       = 2:1\n");
    printf("Push (tie)          = Bet returned\n");

    // Insurance Bet
    printf("\n---Insurance Bet---\n");
    printf("\nOffered when the dealer's upcard is an Ace\n");
    printf("You can place an insurance bet (up to half your original bet)\n");
    printf("If the dealer has a Blackjack, insurance pays 2:1\n");
    printf("If the dealer does not have a Blackjack, you lose the insurance bet\n");

    // Splitting Rules
    printf("\n---Splitting Rules---\n");
    printf("\nIf you have two cards of the same rank, you can split into two hands\n");
    printf("You can place a second bet equal to your original bet\n");
    printf(" - You can split up to 4 times\n");
    printf(" - You can split on Aces\n");
    printf(" - If you split Aces and draw a 10 value card, it does not count as a Blackjack (just a normal 21)\n");
    printf(" - You must play both hands independently\n");

    // Doubling Down
    printf("\n---Doubling Down---\n");
    printf("\nAfter receiving your first two cards, you may double your bet and receive one additional face down card\n");
    printf("You cannot double on a 9, 10, or 11\n");
    printf("After you double, you will automatically stand after the third card\n");

    // Surrender Rule
    printf("\n---Surrender Rule---\n");
    printf("\nYou are only allowed to surrender when the first two cards are drawn\n");
    printf("You give up your hand and recover half your original bet\n");

    // Soft vs Hard Hands
    printf("\n---Soft vs Hard Hands---\n");
    printf("\nSoft Hand: Contains an Ace counted as 11 (e.g., Ace + 6 = Soft 17)\n");
    printf("Hard Hand: No Ace or Ace counts as 1 (e.g., 10-7 = Hard 17 or Ace-10-6 = Hard 17)\n");
}

/////////////////////////////
// 21 Blackjack Game Logic //
/////////////////////////////

// Function that starts the game and manages gameplay flow
void blackjack_start(Card *deck, unsigned long long *uPlayerMoney) {
    int deckIndex = 0;
    int roundNumber = 1;          // Round counter
    unsigned int maxBet = 500;    // Maximum bet
    unsigned int minBet = 10;     // Minimum bet

    srand(time(NULL));    // Initialize random number generator for shuffling

    clear_screen();       // Clear the screen at the start of each round
    while (*uPlayerMoney >= minBet) {    // Continue as long as player has enough money
        initialize_deck(deck);     // Initialize and shuffle deck
        shuffle_deck(deck);
        deckIndex = 0;

        printf("=== Round %d ===\n", roundNumber);   // Display round number
        printf("You have: $%lld\n\n", *uPlayerMoney); // Display current player money

        unsigned int bet;
        // Get valid bet input from the player
        do {
            if (*uPlayerMoney < maxBet) printf("Enter your bet ($%d - $%lld): ", minBet, *uPlayerMoney);
            else printf("Enter your bet ($%d - $%d): ", minBet, maxBet);
            scanf("%d", &bet);
            if ((bet < minBet || bet > maxBet) && bet < *uPlayerMoney) 
                printf("Invalid bet. Please enter an amount between $%d and $%d\n", minBet, maxBet);
            else if (bet > *uPlayerMoney) 
                printf("Invalid bet. Please enter an amount between $%d and $%lld\n", minBet, *uPlayerMoney);
        } while (bet < minBet || bet > maxBet || bet > *uPlayerMoney); // Loop until valid bet is entered

        // Initialize player hands and dealer hand
        Hand playerHand1 = {.count = 0, .bet = bet, .surrendered = 0, .doubled = 0};
        Hand playerHand2 = {.count = 0};
        bool isSplit = false;     // Flag to check if the player has split the hand
        Hand dealerHand = {.count = 0};

        *uPlayerMoney -= bet;    // Deduct bet from the player's money

        clear_screen();  // Clear the screen for the new round
        printf("=== Round %d ===\n", roundNumber);  // Display round number again

        // Deal cards to player and dealer
        deal_card(deck, &deckIndex, &playerHand1);
        deal_card(deck, &deckIndex, &dealerHand);
        deal_card(deck, &deckIndex, &playerHand1);
        deal_card(deck, &deckIndex, &dealerHand);

        // Show the dealer's upcard
        print_hand("Dealer shows", &(Hand){.cards = {dealerHand.cards[0]}, .count = 1});

        // If dealer shows an Ace, offer insurance
        if (strcmp(dealerHand.cards[0].rank, "Ace") == 0) {
            int choice;
            printf("Dealer shows Ace. Take insurance for $50?\n");
            printf("1: Yes\n");
            printf("2: No\n");
            printf("\nEnter Selection: ");
            scanf("%d", &choice);
            if (choice == 1) {
                if (check_blackjack(&dealerHand)) {   // Check if dealer has Blackjack
                    clear_screen();
                    printf("Dealer has Blackjack. Insurance pays 2:1.\n");
                    *uPlayerMoney += 100;   // Pay insurance if dealer has Blackjack
                    goto round_end;
                }
                else {
                    printf("Dealer does not have Blackjack. You lose insurance.\n");
                    printf("Press enter to continue");
                    getchar(); getchar();
                    *uPlayerMoney -= 50;  // Deduct insurance if dealer doesn't have Blackjack
                }
            }
        }

        // If player has Blackjack, handle the payout
        if (check_blackjack(&playerHand1)) {
            if (check_blackjack(&dealerHand)) {
                printf("Both you and dealer have Blackjack. Push.\n");
                *uPlayerMoney += bet;  // Return bet on a tie
            }
            else {
                printf("Blackjack! You win 3:2.\n");
                *uPlayerMoney += bet + (int)(bet * 1.5);  // Player wins Blackjack payout
            }
            goto round_end;
        }

        // If the player has a pair, offer to split the hand
        if (is_pair(&playerHand1)) {
            print_hand("Your hand", &playerHand1);  // Show player hand
            int choice;
            printf("\nYou have a pair. Split?\n");
            printf("1: Yes\n");
            printf("2: No\n");
            printf("\nEnter Selection: ");
            scanf("%d", &choice);
            if (choice == 1) {  // Player chooses to split
                if (*uPlayerMoney < bet) {
                    printf("Not enough money to split.\n");
                }
                else {
                    isSplit = true;

                    *uPlayerMoney -= bet;  // Deduct second bet for split hand
                    playerHand2.cards[0] = playerHand1.cards[1];  // Set second hand's first card
                    playerHand2.count = 1;
                    playerHand2.bet = bet;
                    playerHand2.surrendered = 0;
                    playerHand2.doubled = 0;
                    playerHand2.fromSplit = 1;

                    playerHand1.count = 1;  // Reset first hand after split
                    playerHand1.fromSplit = 1;

                    // Deal additional cards for the split hands
                    deal_card(deck, &deckIndex, &playerHand1);
                    deal_card(deck, &deckIndex, &playerHand2);

                    clear_screen();
                    printf("\n-- Playing Hand 1 of 2 --\n");
                    play_hand(deck, &deckIndex, &playerHand1, uPlayerMoney, roundNumber);  // Play first split hand

                    clear_screen();
                    printf("\n-- Playing Hand 2 of 2 --\n");
                    play_hand(deck, &deckIndex, &playerHand2, uPlayerMoney, roundNumber);  // Play second split hand

                    goto dealer_turn;
                }
            }
        }

        // If no split, play the hand normally
        clear_screen();
        play_hand(deck, &deckIndex, &playerHand1, uPlayerMoney, roundNumber);

        dealer_turn:
        if (playerHand1.surrendered && (!isSplit || playerHand2.surrendered)) {
            goto round_end;
        }

        // Dealer's turn to play
        clear_screen();
        printf("=== Round %d ===\n", roundNumber);
        printf("\nDealer's turn:\n");
        print_hand("Dealer", &dealerHand);
        while (get_hand_value(&dealerHand) < 17) {
            deal_card(deck, &deckIndex, &dealerHand);  // Dealer hits until value is 17 or higher
            print_hand("Dealer", &dealerHand);        // Print the dealer's hand after each hit
        }

        int dealerValue = get_hand_value(&dealerHand);  // Get the dealer's final hand value
        int handsToResolve = isSplit ? 2 : 1;           // Number of hands to resolve (split hands count as 2)
        Hand *hands[] = {&playerHand1, &playerHand2};   // Array of player hands to resolve

        // Resolve each hand (player's hand)
        for (int i = 0; i < handsToResolve; i++) {
            Hand *ph = hands[i];
            if (ph->count == 0 || ph->surrendered) continue;  // Skip surrendered hands

            int playerValue = get_hand_value(ph);  // Get player's hand value
            if (isSplit) {
                printf("\nYour hand %d: ", i + 1);
            }
            else {
                printf("\nYour hand: ");
            }
            print_hand("", ph);   // Print the player's hand
            printf("Your total: %d vs Dealer: %d\n", playerValue, dealerValue);  // Compare hand values

            // Determine the outcome of the hand (win, loss, push)
            if (playerValue > 21) {
                printf("You busted. Lose $%d\n", ph->bet);  // Bust condition
            }
            else if (dealerValue > 21 || playerValue > dealerValue) {
                printf("You win! Gain $%d\n", ph->bet);  // Player wins
                *uPlayerMoney += ph->bet * 2;
            }
            else if (playerValue < dealerValue) {
                printf("Dealer wins. Lose $%d\n", ph->bet);  // Dealer wins
            }
            else {
                printf("Push. No money gained or lost.\n");  // Tie condition
                *uPlayerMoney += ph->bet;
            }
        }

        round_end:  // End of the round
        roundNumber++;  // Increment the round number

        // Check if player has enough money to continue
        if (*uPlayerMoney < minBet) {
            printf("You don't have enough money to continue.\n");
            printf("Press enter to continue.");
            getchar(); getchar();
            break;
        }

        // Ask if the player wants to play another round
        int again;
        printf("\nPlay another round?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("\nEnter Selection: ");
        scanf("%d", &again);  // Get player's choice
        clear_screen();
        if (again != 1) break;  // Exit game if player chooses 'No'
    }
    clear_screen();  // Final screen clear
}


// Function to calculate the total value of a hand (accounting for Ace as 1 or 11)
static int get_hand_value(Hand *hand) {
    int total = 0, aces = 0;  // Initialize total value and ace count
    for (int i = 0; i < hand->count; i++) {  // Loop through all cards in the hand
        // If the card is an Ace, add 11 to total and increase the Ace count
        if (strcmp(hand->cards[i].rank, "Ace") == 0) {
            total += 11;
            aces++;
        }
        // If the card is a face card (King, Queen, or Jack), add 10 to total
        else if (strcmp(hand->cards[i].rank, "King") == 0 ||
                 strcmp(hand->cards[i].rank, "Queen") == 0 ||
                 strcmp(hand->cards[i].rank, "Jack") == 0) {
            total += 10;
        }
        // If the card is a numbered card (2-10), add its integer value to total
        else {
            total += atoi(hand->cards[i].rank);  // Convert rank to an integer and add to total
        }
    }
    // Adjust total if the hand exceeds 21 and there are Aces, making them worth 1 instead of 11
    while (total > 21 && aces > 0) {
        total -= 10;  // Convert an Ace from 11 to 1
        aces--;        // Decrease the Ace count
    }
    return total;  // Return the final total value of the hand
}

//////////////////////
// Helper Functions //
//////////////////////

// Function to deal a card from the deck to a hand
static void deal_card(Card *deck, int *deckIndex, Hand *hand) {
    if (*deckIndex >= DECK_SIZE) {  // Check if there are cards left in the deck
        printf("Deck out of cards!\n");  // Print an error message if no cards are left
        return;
    }
    // Add the next card from the deck to the hand and increment the hand's card count
    hand->cards[hand->count++] = deck[(*deckIndex)++];
}

// Function to print the cards in a hand
static void print_hand(const char *name, Hand *hand) {
    if (name && strlen(name) > 0) {  // If the 'name' parameter is non-empty
        printf("%s: ", name);  // Print the name (e.g., "Your hand")
    }
    // Loop through each card in the hand and print its rank and suit
    for (int i = 0; i < hand->count; i++) {
        printf("[%s of %s] ", hand->cards[i].rank, hand->cards[i].suit);  // Print the card in the format "Rank of Suit"
    }
    printf("\n");  // Newline after printing the cards
}

// Function to check if the hand contains a pair (two cards with the same rank)
static int is_pair(Hand *hand) {
    // Return true (1) if the hand has exactly 2 cards and the ranks match
    return hand->count == 2 && strcmp(hand->cards[0].rank, hand->cards[1].rank) == 0;
}

// Function to check if the hand is a Blackjack (Ace + 10-value card, exactly 2 cards)
static int check_blackjack(Hand *hand) {
    // Return true (1) if the hand has exactly 2 cards and the value is 21 (Blackjack)
    return hand->count == 2 && get_hand_value(hand) == 21;
}

// Function to handle playing a hand, including player actions (Hit, Stand, Surrender, Double Down)
static void play_hand(Card *deck, int *deckIndex, Hand *hand, unsigned long long *uPlayerMoney, int roundNumber) {
    int choice;          // Store player's action choice
    int firstTurn = 1;   // Flag to indicate whether it's the player's first turn

    while (1) {  // Infinite loop to continue playing until the player chooses to stand or bust
        printf("=== Round %d ===\n\n", roundNumber);  // Display the round number
        printf("-- Playing Your Hand --\n");
        print_hand("Your hand", hand);  // Print the player's current hand
        int value = get_hand_value(hand);  // Get the total value of the hand
        printf("Current total: %d\n", value);  // Display the current total value of the hand

        // If the total value exceeds 21 (bust), end the hand
        if (value > 21) {
            printf("You busted.\n");
            break;  // End the loop if the player busts
        }

        // Display available actions for the player
        printf("\n1: Hit\n2: Stand\n");
        if (firstTurn) {  // If it's the player's first turn, allow Surrender and Double Down
            printf("3: Surrender (-50%%)\n4: Double Down\n");
        }
        printf("Enter Selection: ");
        scanf("%d", &choice);  // Get the player's action choice

        // Process the player's action based on their choice
        switch (choice) {
            case 1:  // Player chooses to "Hit" (take another card)
                deal_card(deck, deckIndex, hand);  // Deal one more card to the player's hand
                firstTurn = 0;  // Mark that it's no longer the first turn (for surrender and double down)
                break;
            case 2:  // Player chooses to "Stand" (stop taking cards)
                return;  // Exit the loop and end the hand
            case 3:  // Player chooses to "Surrender" (give up the hand)
                if (!firstTurn || hand->fromSplit) {  // Check if surrender is allowed
                    printf("Surrender is only allowed at the start and not after a split.\n");
                    break;
                }
                printf("You surrendered. Lose half your bet.\n");
                hand->surrendered = 1;  // Mark the hand as surrendered
                *uPlayerMoney -= hand->bet / 2;  // Deduct half the bet from the player's money
                hand->count = 0;  // Reset the hand (no cards left)
                return;  // End the hand
            case 4:  // Player chooses to "Double Down" (double their bet and take one more card)
                if (!firstTurn) {  // Check if it's the player's first turn
                    printf("You can only double down on your first move.\n");
                    break;
                }
                if (*uPlayerMoney < hand->bet) {  // Check if the player has enough money to double down
                    printf("Not enough money to double down.\n");
                    break;
                }
                printf("Doubling down.\n");
                *uPlayerMoney -= hand->bet;  // Deduct the additional bet from the player's money
                hand->bet *= 2;  // Double the player's bet
                deal_card(deck, deckIndex, hand);  // Deal one more card to the player's hand
                print_hand("Your hand after double down", hand);  // Print the updated hand
                return;  // End the hand (player automatically stands after doubling down)
            default:  // Invalid option
                clear_screen();  // Clear the screen and prompt the player again
        }
        clear_screen();  // Clear the screen after each action
    }
}
