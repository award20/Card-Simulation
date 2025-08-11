/*
 * @author: Anthony Ward
 * @upload date: 08/11/2025
 *
 * Idiot card game implementation.
 * Handles game logic, player/ai turns, and display.
 * 
 * Pure C implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "deck.h"
#include "idiot.h"

//////////////////////////////
// How to play instructions //
//////////////////////////////

// Display how to play instructions for Idiot
void idiotHowToPlay() {
    clear_screen();
    printf("=== HOW TO PLAY: IDIOT ===\n\n");
    printf("- Each player has 3 face-down, 3 face-up, and 3 hand cards.\n");
    printf("- Take turns playing cards onto the pile.\n");
    printf("- Card must be equal or higher in value than the top card.\n");
    printf("- Special cards:\n");
    printf("   2: Resets the pile, anything can be played on it.\n");
    printf("   3: Mirrors the card below it.\n");
    printf("   10: Burns the pile (removes all cards).\n");
    printf("- 4 of the same card in a row also burns the pile.\n");
    printf("- If you can't play, take the pile.\n");
    printf("- First to play all cards wins.\n\n");
    printf("Press Enter to continue...");
    getchar(); getchar();
    clear_screen();
}

//////////////////////
// Idiot Game Logic //
//////////////////////

// Function that starts the game and manages gameplay flow
void idiot_start(unsigned long long *uPlayerMoney) {
    // Prepare the deck and core game actors/state containers
    Card deck[DECK_SIZE];  // Array to hold the deck of cards
    initialize_deck(deck);  // Initialize the deck with cards
    IdiotPlayer player = {0}, ai = {0};
    CardPile drawPile = {0}, wastePile = {0};
    int difficulty;
    int currentPlayer = 0;
    AILastMove aiLastMove = {0};
    PlayerLastMove playerLastMove = {0};

    // Difficulty selection ui
    clear_screen();
    printf("Select Opponent Difficulty:\n");
    printf("1. Easy\n");
    printf("2. Normal\n");
    printf("3. Hard\n");
    printf("> ");
    scanf("%d", &difficulty);

    // Betting system for Normal and Hard difficulties; Easy has no betting
    int easy = 1;
    int normal = 2;
    int hard = 3;
    unsigned int bet = 0;
    unsigned int minBet = 10;
    unsigned int maxBet = (difficulty == normal) ? 100 : 500;

    // Prompt for a valid bet if not playing Easy
    if (difficulty != easy) {
        printf("Place your bet ($%d-$%d): ", minBet, maxBet);
        while (1) {
            scanf("%d", &bet);
            if (bet >= minBet && bet <= maxBet && bet <= *uPlayerMoney) break;
            printf("Invalid bet. Enter a value between $%d and $%d: ", minBet, maxBet);
        }
        *uPlayerMoney -= bet;
    }

    // Shuffle the deck
    shuffle_deck(deck);

    // Initialize player and AI face-down and face-up cards
    for (int i = 0; i < FACE_DOWN_SIZE; i++) {
        player.faceDown[i] = deck[i];
        ai.faceDown[i] = deck[i + 3];
    }
    for (int i = 0; i < FACE_UP_SIZE; i++) {
        player.faceUp[i] = deck[i + 6];
        player.faceUp[i].revealed = 1;
        ai.faceUp[i] = deck[i + 9];
        ai.faceUp[i].revealed = 1;
    }
    for (int i = 0; i < HAND_SIZE; i++) {
        player.hand[i] = deck[i + 12];
        ai.hand[i] = deck[i + 15];
    }

    // Set hand counts
    player.handCount = ai.handCount = HAND_SIZE;
    player.faceUpCount = ai.faceUpCount = FACE_UP_SIZE;
    player.faceDownCount = ai.faceDownCount = FACE_DOWN_SIZE;

    int initializeDrawPile = 18;
    // Initialize draw pile
    for (int i = initializeDrawPile; i < DECK_SIZE; i++) {
        drawPile.pile[drawPile.count++] = deck[i];
    }

    swap_hand_cards(&player);

    // Determine first player based on difficulty
    if (difficulty == easy) currentPlayer = 0;
    else if (difficulty == hard) currentPlayer = 1;
    else currentPlayer = rand() % 2;

    // Main game loop
    while (1) {
        display_idiot_game(&player, &ai, &drawPile, &wastePile, &aiLastMove);

        IdiotPlayer *current = (currentPlayer == 0) ? &player : &ai;
        Card *topCard = (wastePile.count > 0) ? &wastePile.pile[wastePile.count - 1] : NULL;

        int totalPlayableCards = current->handCount;
        if (current->handCount == 0 && current->faceUpCount > 0) {
            totalPlayableCards = current->faceUpCount;
        }
        else if (current->handCount == 0 && current->faceUpCount == 0 && drawPile.count == 0) {
            if (current->faceDownCount > 0) {
                if (currentPlayer == 0)
                    printf("\nYou have no cards in hand and in the face-up piles. You may now play your face-down cards.\n");
                totalPlayableCards = current->faceDownCount;
            }
            else {
                if (currentPlayer == 0)
                    printf("\nYou have no more cards left to play. Game over.\n");
                break;
            }
        }

        if (currentPlayer == 0) {
            int choice;
            while (1) {
                printf("\nYour turn. Select card to play (1-%d), or 0 to take pile: ", totalPlayableCards);
                if (scanf("%d", &choice) != 1) {
                    while (getchar() != '\n'); // Clear invalid input
                    printf("Invalid selection\n");
                    continue;
                }
                if (choice < 0 || choice > totalPlayableCards) {
                    printf("Invalid selection\n");
                    continue;
                }
                break;
            }

            if (choice >= 1 && choice <= totalPlayableCards) {
                Card selected;
                int validMove = 0;

                // Hand phase
                if (current->handCount > 0 && choice <= current->handCount) {
                    selected = current->hand[choice - 1];
                    validMove = (topCard == NULL || can_play_card(topCard, &selected, &wastePile));
                    if (validMove) {
                        for (int j = choice - 1; j < current->handCount - 1; j++) {
                            current->hand[j] = current->hand[j + 1];
                        }
                        playerLastMove.playedCount = 0;
                        current->handCount--;
                    }
                }

                // Face-up phase
                else if (current->faceUpCount > 0 && choice <= current->faceUpCount) {
                    selected = current->faceUp[choice - 1];
                    validMove = (topCard == NULL || can_play_card(topCard, &selected, &wastePile));
                    if (validMove) {
                        for (int j = choice - 1; j < current->faceUpCount - 1; j++) {
                            current->faceUp[j] = current->faceUp[j + 1];
                        }
                        playerLastMove.playedCount = 0;
                        current->faceUpCount--;
                    }
                }
                // Face-down phase
                else if (current->faceDownCount > 0) {
                    // Always reveal and attempt to play the selected face-down card
                    selected = current->faceDown[choice - 1];
                    // Remove the card from faceDown
                    for (int j = choice - 1; j < current->faceDownCount - 1; j++) {
                        current->faceDown[j] = current->faceDown[j + 1];
                    }
                    current->faceDownCount--;

                    // Always add to playerLastMove for display
                    playerLastMove.playedCount = 0;
                    playerLastMove.played[playerLastMove.playedCount++] = selected;

                    // Check if it is a valid play
                    int validMove = (topCard == NULL || can_play_card(topCard, &selected, &wastePile));
                    if (validMove) {
                        wastePile.pile[wastePile.count++] = selected;
                        draw_from_pile(current, &drawPile);

                        // Burning the pile and getting another turn
                        if (is_special_card(&selected, 10) || is_four_of_a_kind(&wastePile)) {
                            burn_pile(&wastePile);
                            continue;
                        }

                        // Playing a 2 and getting another turn
                        if (is_special_card(&selected, 2)) {
                            continue;
                        }

                        // Playing a 3 and mirroring
                        if (is_special_card(&selected, 3)) {
                            Card *mirroredCard = find_mirrored_card(&wastePile);
                            if (mirroredCard) {
                                printf("[%s of %s]\n", mirroredCard->rank, mirroredCard->suit);
                            }
                            else {
                                printf("[none]\n");
                            }
                        }
                    }
                    else {
                        // Pick up the pile, including the revealed card
                        wastePile.pile[wastePile.count++] = selected;
                        handle_pile_pickup(current, &wastePile);
                        continue; // Player gets another turn after picking up
                    }
                }

                if (validMove) playerLastMove.played[playerLastMove.playedCount++] = selected;
                else {
                    continue;
                }

                // Check how many of the same card are in hand
                int additionalCount = 0;
                for (int i = 0; i < current->handCount; i++) {
                    if (strcmp(current->hand[i].rank, selected.rank) == 0) {
                        additionalCount++;
                    }
                }

                if (additionalCount > 0) {
                    printf("You have %d additional %s's, would you like to play extra? (0-%d):\n", additionalCount, selected.rank, additionalCount);
                    int extraChoice;
                    scanf("%d", &extraChoice);
                    extraChoice = (extraChoice > additionalCount) ? additionalCount : extraChoice;

                    for (int i = 0; i < extraChoice; i++) {
                        for (int j = 0; j < current->handCount; j++) {
                            if (strcmp(current->hand[j].rank, selected.rank) == 0) {
                                wastePile.pile[wastePile.count++] = current->hand[j];
                                playerLastMove.played[playerLastMove.playedCount++] = current->hand[j];
                                for (int k = j; k < current->handCount - 1; k++) {
                                    current->hand[k] = current->hand[k + 1];
                                }
                                current->handCount--;
                                break;
                            }
                        }
                    }
                    draw_from_pile(current, &drawPile);
                    sort_hand_low_to_high(current);
                }

                // After playing cards, refill hand to 3
                if (current->handCount < 3 && drawPile.count > 0) {
                    int drawCount = 3 - current->handCount;
                    drawCount = (drawCount > drawPile.count) ? drawPile.count : drawCount;
                    for (int i = 0; i < drawCount; i++) {
                        current->hand[current->handCount++] = drawPile.pile[--drawPile.count];
                    }
                    sort_hand_low_to_high(current);
                    printf("\nYou drew %d card(s) to refill your hand.\n", drawCount);
                }

                if (topCard == NULL || can_play_card(topCard, &selected, &wastePile)) {
                    wastePile.pile[wastePile.count++] = selected;
                    draw_from_pile(current, &drawPile);

                    // Burning the pile and getting another turn
                    if (is_special_card(&selected, 10) || is_four_of_a_kind(&wastePile)) {
                        burn_pile(&wastePile);
                        continue;
                    }

                    // Playing a 2 and getting another turn
                    if (is_special_card(&selected, 2)) {
                        continue;
                    }

                    // Playing a 3 and mirroring
                    if (is_special_card(&selected, 3)) {
                        Card *mirroredCard = find_mirrored_card(&wastePile);
                        if (mirroredCard) {
                            printf("[%s of %s]\n", mirroredCard->rank, mirroredCard->suit);
                        }
                        else {
                            printf("[none]\n");
                        }
                    }

                }
                else {
                    // If the player can't play the last face-down card, do NOT declare a win, just continue
                    continue;
                }
            }
            else if (choice == 0) {
                handle_pile_pickup(current, &wastePile);
                continue; // Player gets another turn
            }

        }
        else {
            ai_play(&ai, &wastePile, &drawPile, difficulty, &aiLastMove);

            // If the AI burned the pile, give it another turn
            if (aiLastMove.burned) {
                continue; // AI gets another turn
            }
            // If the AI played a 2, give it another turn
            if (aiLastMove.playedCount > 0 && is_special_card(&aiLastMove.played[0], 2)) {
                continue; // AI gets another turn
            }
            // If the AI picked up the pile, give it another turn
            if (aiLastMove.playedCount == 0) {
                continue; // AI gets another turn
            }
        }

        // Check for winner (only after a successful play)
        if (current->handCount == 0 && current->faceUpCount == 0 && current->faceDownCount == 0) {
            printf("\n%s wins the game!\n", currentPlayer == 0 ? "Player" : "Opponent");
            if (currentPlayer == 0) *uPlayerMoney += bet * (difficulty == normal ? 2 : (difficulty == hard ? 5 : 1));
            break;
        }

        currentPlayer = !currentPlayer;
    }
}

//////////////////////
// Helper Functions //
//////////////////////

// Function to handle picking up the pile when a player cannot play
static void handle_pile_pickup(IdiotPlayer *p, CardPile *pile) {
    
    // Add cards to player's hand
    for (int i = 0; i < pile->count; i++) {
        p->hand[p->handCount++] = pile->pile[i];
    }
    pile->count = 0;  // Empty the pile
    sort_hand_low_to_high(p);
}

// Function to find the card value
static int card_value(const Card *card) {
    if (strcmp(card->rank, "Ace") == 0) return 14;
    if (strcmp(card->rank, "King") == 0) return 13;
    if (strcmp(card->rank, "Queen") == 0) return 12;
    if (strcmp(card->rank, "Jack") == 0) return 11;
    return atoi(card->rank);
}

// Function to check if a card is a special card (2, 3, or 10)
static int is_special_card(const Card *card, int value) {
    return card_value(card) == value;
}

// Function to check if a card can be played on top of another card
static int can_play_card(const Card *top, const Card *next, const CardPile *wastePile) {
    if (is_special_card(next, 2)) return 1;
    if (is_special_card(next, 3)) return 1;
    if (is_special_card(next, 10)) return 1;

    if (top == NULL) return 1;

    // Mirror effect: if the top card is a 3, check if we can play the card below it
    if (is_special_card(top, 3)) {
        if (wastePile && wastePile->count > 1) {
            int idx = wastePile->count - 2;
            while (idx >= 0 && is_special_card(&wastePile->pile[idx], 3)) {
                idx--;
            }
            if (idx >= 0) {
                return can_play_card(&wastePile->pile[idx], next, wastePile);
            }
            else {
                return 1; // Only 3s in pile, allow any card
            }
        }
        else {
            return 1;
        }
    }
    // Normal play: card must be equal or higher in value than the top card
    return card_value(next) >= card_value(top);
}

// Function to determine if 4 of a kind is present in the pile
static int is_four_of_a_kind(CardPile *pile) {
    if (pile->count < 4) return 0;
    Card *last = &pile->pile[pile->count - 1];
    for (int i = 2; i < 5; i++) {
        if (strcmp(pile->pile[pile->count - i].rank, last->rank) != 0) return 0;
    }
    return 1;
}

// Function to burn the pile (remove all cards)
static void burn_pile(CardPile *pile) {
    pile->count = 0;
}

// Function to draw cards from the draw pile
static void draw_from_pile(IdiotPlayer *p, CardPile *drawPile) {
    while (p->handCount < HAND_SIZE && drawPile->count > 0) {
        p->hand[p->handCount++] = drawPile->pile[--drawPile->count];
    }
}

// Function to swap cards between hand and face-up pile in the beginning
static void swap_hand_cards(IdiotPlayer *p) {
    while (1) {
        clear_screen();
        printf("--- Swap Cards ---\n\n");
        for (int i = 0; i < HAND_SIZE; i++) {
            printf("[%s of %s] ", p->hand[i].rank, p->hand[i].suit);
        }
        printf("\n\n");
        for (int i = 0; i < FACE_UP_SIZE; i++) {
            printf("[%s of %s] ", p->faceUp[i].rank, p->faceUp[i].suit);
        }

        printf("\n\nWould you like to replace a Face-Up card?\n");
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");
        int option;
        scanf("%d", &option);
        if (option != 1) break;

        printf("\n--- Swap Cards ---\n\n");
        for (int i = 0; i < HAND_SIZE; i++) {
            printf("[%s of %s] ", p->hand[i].rank, p->hand[i].suit);
        }
        printf("\n\n");
        for (int i = 0; i < FACE_UP_SIZE; i++) {
            printf("[%s of %s] ", p->faceUp[i].rank, p->faceUp[i].suit);
        }

        printf("\n\nWhich Card would you like to replace?\n");
        for (int i = 0; i < FACE_UP_SIZE; i++) {
            printf("%d: [%s of %s]\n", i + 1, p->faceUp[i].rank, p->faceUp[i].suit);
        }
        printf("4: Cancel\n> ");
        int faceIndex;
        scanf("%d", &faceIndex);
        if (faceIndex < 1 || faceIndex > 3) continue;
        faceIndex--;

        printf("\n--- Swap Cards ---\n\n");
        for (int i = 0; i < FACE_UP_SIZE; i++) {
            printf("[%s of %s] ", p->faceUp[i].rank, p->faceUp[i].suit);
        }
        printf("\n\n");
        for (int i = 0; i < HAND_SIZE; i++) {
            printf("[%s of %s] ", p->hand[i].rank, p->hand[i].suit);
        }

        printf("\n\nWhat card would you like to replace [%s of %s] with?\n",
               p->faceUp[faceIndex].rank, p->faceUp[faceIndex].suit);
        for (int i = 0; i < HAND_SIZE; i++) {
            printf("%d: [%s of %s]\n", i + 1, p->hand[i].rank, p->hand[i].suit);
        }
        printf("4: Cancel\n> ");
        int handChoice;
        scanf("%d", &handChoice);
        if (handChoice < 1 || handChoice > 3) continue;
        handChoice--;

        Card temp = p->faceUp[faceIndex];
        p->faceUp[faceIndex] = p->hand[handChoice];
        p->hand[handChoice] = temp;
    }
    sort_hand_low_to_high(p);
}

// Function to automatically sort the player's hand from low to high (helps with readability)
static void sort_hand_low_to_high(IdiotPlayer *p) {
    for (int i = 0; i < p->handCount - 1; i++) {
        for (int j = i + 1; j < p->handCount; j++) {
            if (card_value(&p->hand[i]) > card_value(&p->hand[j])) {
                Card temp = p->hand[i];
                p->hand[i] = p->hand[j];
                p->hand[j] = temp;
            }
        }
    }
}

// Function that controls ai turn logic
static void ai_play(IdiotPlayer *ai, CardPile *pile, CardPile *drawPile, int difficulty, AILastMove *lastMove) {
    int normal = 2;
    Card *top = pile->count > 0 ? &pile->pile[pile->count - 1] : NULL;
    lastMove->playedCount = 0;
    lastMove->burned = 0;
    lastMove->mirrored = 0;
    lastMove->mirroredCard = NULL;

    // Indexes for special cards and lowest value card
    int idx2 = -1, idx10 = -1, idx3 = -1, idxLowest = -1, lowestVal = 100;

    // Scan hand for playable cards and specials
    for (int i = 0; i < ai->handCount; i++) {
        if (can_play_card(top, &ai->hand[i], pile)) {
            int val = card_value(&ai->hand[i]);
            if (val < lowestVal) {
                lowestVal = val;
                idxLowest = i;
            }
            if (is_special_card(&ai->hand[i], 2)) idx2 = i;
            if (is_special_card(&ai->hand[i], 3)) idx3 = i;
            if (is_special_card(&ai->hand[i], 10)) idx10 = i;
        }
    }

    // --- HAND PHASE: Play a 2, then immediately play another card if possible ---
    if (idx2 != -1) {
        Card played2 = ai->hand[idx2];
        pile->pile[pile->count++] = played2;
        lastMove->played[0] = played2;
        int playedCount = 1;
        for (int j = idx2; j < ai->handCount - 1; j++) {
            ai->hand[j] = ai->hand[j + 1];
        }
        ai->handCount--;
        draw_from_pile(ai, drawPile);

        // Now, immediately play another card if possible
        Card *top2 = pile->count > 0 ? &pile->pile[pile->count - 1] : NULL;
        int idxNext = -1, idxNext3 = -1, idxNext10 = -1, lowestVal2 = 100;
        for (int i = 0; i < ai->handCount; i++) {
            if (can_play_card(top2, &ai->hand[i], pile)) {
                int val = card_value(&ai->hand[i]);
                if (val < lowestVal2) {
                    lowestVal2 = val;
                    idxNext = i;
                }
                if (is_special_card(&ai->hand[i], 3)) idxNext3 = i;
                if (is_special_card(&ai->hand[i], 10)) idxNext10 = i;
            }
        }
        // Prefer lowest non-special, then 3, then 10
        if (idxNext != -1 && idxNext != idxNext3 && idxNext != idxNext10) {
            Card playedNext = ai->hand[idxNext];
            pile->pile[pile->count++] = playedNext;
            lastMove->played[playedCount++] = playedNext;
            for (int j = idxNext; j < ai->handCount - 1; j++) {
                ai->hand[j] = ai->hand[j + 1];
            }
            ai->handCount--;
            draw_from_pile(ai, drawPile);
        } else if (idxNext3 != -1) {
            Card playedNext = ai->hand[idxNext3];
            pile->pile[pile->count++] = playedNext;
            lastMove->played[playedCount++] = playedNext;
            for (int j = idxNext3; j < ai->handCount - 1; j++) {
                ai->hand[j] = ai->hand[j + 1];
            }
            ai->handCount--;
            lastMove->mirrored = 1;
            lastMove->mirroredCard = find_mirrored_card(pile);
            draw_from_pile(ai, drawPile);
        } else if (idxNext10 != -1) {
            Card playedNext = ai->hand[idxNext10];
            pile->pile[pile->count++] = playedNext;
            lastMove->played[playedCount++] = playedNext;
            for (int j = idxNext10; j < ai->handCount - 1; j++) {
                ai->hand[j] = ai->hand[j + 1];
            }
            ai->handCount--;
            lastMove->burned = 1;
            burn_pile(pile);
            draw_from_pile(ai, drawPile);
        }
        lastMove->playedCount = playedCount;
        return;
    }

    // --- HAND PHASE: Play multiple cards of the same rank ---
    if (idxLowest != -1 && idxLowest != idx3 && idxLowest != idx10 && idxLowest != idx2) {
        Card played = ai->hand[idxLowest];
        pile->pile[pile->count++] = played;
        lastMove->played[0] = played;
        int playedCount = 1;
        for (int j = idxLowest; j < ai->handCount - 1; j++) {
            ai->hand[j] = ai->hand[j + 1];
        }
        ai->handCount--;

        // Play additional cards of the same rank if available (normal/hard)
        if (difficulty >= normal) {
            int i = 0;
            while (i < ai->handCount && playedCount < 4) {
                if (strcmp(ai->hand[i].rank, played.rank) == 0) {
                    pile->pile[pile->count++] = ai->hand[i];
                    lastMove->played[playedCount++] = ai->hand[i];
                    for (int k = i; k < ai->handCount - 1; k++) {
                        ai->hand[k] = ai->hand[k + 1];
                    }
                    ai->handCount--;
                }
                else i++;
            }
        }
        lastMove->playedCount = playedCount;
        draw_from_pile(ai, drawPile);
        return;
    }

    // --- HAND PHASE: Play 3 ---
    if (idx3 != -1) {
        Card played = ai->hand[idx3];
        pile->pile[pile->count++] = played;
        lastMove->played[0] = played;
        lastMove->playedCount = 1;
        for (int j = idx3; j < ai->handCount - 1; j++) {
            ai->hand[j] = ai->hand[j + 1];
        }
        ai->handCount--;
        lastMove->mirrored = 1;
        lastMove->mirroredCard = find_mirrored_card(pile);
        draw_from_pile(ai, drawPile);
        return;
    }

    // --- HAND PHASE: Play 10 ---
    if (idx10 != -1) {
        Card played = ai->hand[idx10];
        pile->pile[pile->count++] = played;
        lastMove->played[0] = played;
        lastMove->playedCount = 1;
        for (int j = idx10; j < ai->handCount - 1; j++) {
            ai->hand[j] = ai->hand[j + 1];
        }
        ai->handCount--;
        lastMove->burned = 1;
        burn_pile(pile);
        draw_from_pile(ai, drawPile);
        return;
    }

    // --- FACE-UP PHASE ---
    if (ai->handCount == 0 && ai->faceUpCount > 0) {
        int idx2 = -1, idx10 = -1, idx3 = -1, idxLowest = -1, lowestVal = 100;
        Card *top = pile->count > 0 ? &pile->pile[pile->count - 1] : NULL;

        // Scan face-up for playable cards and specials
        for (int i = 0; i < ai->faceUpCount; i++) {
            if (can_play_card(top, &ai->faceUp[i], pile)) {
                int val = card_value(&ai->faceUp[i]);
                if (val < lowestVal) {
                    lowestVal = val;
                    idxLowest = i;
                }
                if (is_special_card(&ai->faceUp[i], 2)) idx2 = i;
                if (is_special_card(&ai->faceUp[i], 3)) idx3 = i;
                if (is_special_card(&ai->faceUp[i], 10)) idx10 = i;
            }
        }

        // Play 2
        if (idx2 != -1) {
            Card played = ai->faceUp[idx2];
            pile->pile[pile->count++] = played;
            lastMove->played[0] = played;
            int playedCount = 1;
            for (int j = idx2; j < ai->faceUpCount - 1; j++) {
                ai->faceUp[j] = ai->faceUp[j + 1];
            }
            ai->faceUpCount--;
            lastMove->playedCount = playedCount;
            return;
        }
        // Play multiple of same rank
        if (idxLowest != -1 && idxLowest != idx3 && idxLowest != idx10 && idxLowest != idx2) {
            Card played = ai->faceUp[idxLowest];
            pile->pile[pile->count++] = played;
            lastMove->played[0] = played;
            int playedCount = 1;
            for (int j = idxLowest; j < ai->faceUpCount - 1; j++) {
                ai->faceUp[j] = ai->faceUp[j + 1];
            }
            ai->faceUpCount--;

            if (difficulty >= normal) {
                int i = 0;
                while (i < ai->faceUpCount && playedCount < 4) {
                    if (strcmp(ai->faceUp[i].rank, played.rank) == 0) {
                        pile->pile[pile->count++] = ai->faceUp[i];
                        lastMove->played[playedCount++] = ai->faceUp[i];
                        for (int k = i; k < ai->faceUpCount - 1; k++) {
                            ai->faceUp[k] = ai->faceUp[k + 1];
                        }
                        ai->faceUpCount--;
                    }
                    else i++;
                }
            }
            lastMove->playedCount = playedCount;
            return;
        }
        // Play 3
        if (idx3 != -1) {
            Card played = ai->faceUp[idx3];
            pile->pile[pile->count++] = played;
            lastMove->played[0] = played;
            lastMove->playedCount = 1;
            for (int j = idx3; j < ai->faceUpCount - 1; j++) {
                ai->faceUp[j] = ai->faceUp[j + 1];
            }
            ai->faceUpCount--;
            lastMove->mirrored = 1;
            lastMove->mirroredCard = find_mirrored_card(pile);
            return;
        }
        // Play 10
        if (idx10 != -1) {
            Card played = ai->faceUp[idx10];
            pile->pile[pile->count++] = played;
            lastMove->played[0] = played;
            lastMove->playedCount = 1;
            for (int j = idx10; j < ai->faceUpCount - 1; j++) {
                ai->faceUp[j] = ai->faceUp[j + 1];
            }
            ai->faceUpCount--;
            lastMove->burned = 1;
            burn_pile(pile);
            return;
        }

        // If nothing playable in face-up, pick up the pile
        for (int i = 0; i < pile->count; i++)
            ai->hand[ai->handCount++] = pile->pile[i];
        pile->count = 0;
        lastMove->playedCount = 0;
        lastMove->burned = 0;
        lastMove->mirrored = 0;
        lastMove->mirroredCard = NULL;
        return;
    }

    // --- FACE-DOWN PHASE ---
    if (ai->handCount == 0 && ai->faceUpCount == 0 && ai->faceDownCount > 0) {
        Card playedCard = ai->faceDown[0];
        // Remove from faceDown
        for (int j = 0; j < ai->faceDownCount - 1; j++) {
            ai->faceDown[j] = ai->faceDown[j + 1];
        }
        ai->faceDownCount--;

        // Always reveal and attempt to play the card
        lastMove->played[0] = playedCard;
        lastMove->playedCount = 1;

        if (can_play_card(top, &playedCard, pile)) {
            pile->pile[pile->count++] = playedCard;
            if (is_special_card(&playedCard, 10) || is_four_of_a_kind(pile)) {
                lastMove->burned = 1;
                burn_pile(pile);
            }
            if (is_special_card(&playedCard, 3)) {
                lastMove->mirrored = 1;
                lastMove->mirroredCard = find_mirrored_card(pile);
            }
            return;
        }
        else {
            // Can't play, must pick up the pile (including the revealed card)
            for (int i = 0; i < pile->count; i++)
                ai->hand[ai->handCount++] = pile->pile[i];
            ai->hand[ai->handCount++] = playedCard;
            pile->count = 0;
            lastMove->playedCount = 0;
            lastMove->burned = 0;
            lastMove->mirrored = 0;
            lastMove->mirroredCard = NULL;
            return;
        }
    }

    // If nothing playable, pick up the pile
    if (idxLowest == -1 && idx2 == -1 && idx3 == -1 && idx10 == -1) {
        for (int i = 0; i < pile->count; i++)
            ai->hand[ai->handCount++] = pile->pile[i];
        pile->count = 0;
        lastMove->playedCount = 0;
        lastMove->burned = 0;
        lastMove->mirrored = 0;
        lastMove->mirroredCard = NULL;
        return;
    }
}

// Function to find the mirrored card in the waste pile
static Card* find_mirrored_card(CardPile *pile) {
    // Start below the top card and find the last non-3 card
    int idx = pile->count - 2; // Start below the top card
    while (idx >= 0 && is_special_card(&pile->pile[idx], 3)) {
        idx--;
    }
    return (idx >= 0) ? &pile->pile[idx] : NULL; // Return NULL if no valid card is found
}

// Function to display the game board
static void display_idiot_game(IdiotPlayer *p, IdiotPlayer *ai, CardPile *drawPile, CardPile *wastePile, AILastMove *lastMove) {
    int aiMaxHandDisplay = 6;
    clear_screen();

    // Always show the AI's last move if there was one
    if (lastMove && (lastMove->playedCount > 0 || lastMove->burned || lastMove->mirrored)) {
        if (lastMove->playedCount > 0) {
            for (int i = 0; i < lastMove->playedCount; i++) {
                printf("The opponent played a [%s of %s]", lastMove->played[i].rank, lastMove->played[i].suit);
                // Show mirroring for 3s
                if (is_special_card(&lastMove->played[i], 3)) {
                    if (lastMove->mirroredCard) {
                        printf(" (Mirroring: [%s of %s])", lastMove->mirroredCard->rank, lastMove->mirroredCard->suit);
                    }
                    else {
                        printf(" (Mirroring: [none])");
                    }
                }
                printf("\n");
            }
        }
        if (lastMove->burned) {
            printf("The opponent burned the pile!\n");
        }
        if (lastMove->playedCount == 0) {
            printf("The opponent takes the pile.\n");
        }
    }

    // Opponents hand
    printf("\n--- Opponent ---\n");
    // AI face-down cards
    for (int i = 0; i < ai->faceDownCount; i++) {
        printf("[???] ");
    }
    printf("\n");
    // AI face-up cards
    for (int i = 0; i < ai->faceUpCount; i++) {
        printf("[%s of %s] ", ai->faceUp[i].rank, ai->faceUp[i].suit);
    }
    printf("\n");
    // AI hand cards
    if (ai->handCount > aiMaxHandDisplay) {
        for (int i = 0; i < aiMaxHandDisplay; i++) {
            printf("[???] ");
        }
        printf("...\n");
    }
    else {
        for (int i = 0; i < ai->handCount; i++) {
            printf("[???] ");
        }
        printf("\n");
    }
    printf("(%d cards in hand)\n\n", ai->handCount);

    printf("Draw Pile: %d cards\n", drawPile->count);

    // Waste Pile
    if (wastePile->count > 0) {
        Card *top = &wastePile->pile[wastePile->count - 1];
        printf("Waste Pile: [%s of %s]\n", top->rank, top->suit);
        if (is_special_card(top, 3)) {
            Card *mirroredCard = find_mirrored_card(wastePile);
            if (mirroredCard) {
                printf("   (Mirroring: [%s of %s])\n", mirroredCard->rank, mirroredCard->suit);
            }
            else {
                printf("   (Mirroring: [none])\n");
            }
        }
    }
    else {
        printf("Waste Pile: [empty]\n");
    }

    // Players hand
    printf("\n--- Your Hand ---\n");
    for (int i = 0; i < p->handCount; i++) {
        printf("[%s of %s] ", p->hand[i].rank, p->hand[i].suit);
    }
    printf("\n\n");
    for (int i = 0; i < p->faceUpCount; i++) {
        printf("[%s of %s] ", p->faceUp[i].rank, p->faceUp[i].suit);
    }
    printf("\n");
    for (int i = 0; i < p->faceDownCount; i++) {
        printf("[???] ");
    }
    printf("\n\n");
}

/*
 * This game was annoyingly difficult to implement. Also this isn't technically an AI since its just a state machine
 * that plays cards based on the current state of the game. It doesn't learn or adapt, but it does play based on the
 * current game rules and the cards available to it. Once I have a more complex understanding of how ML works, I may
 * implement a more advanced AI that can learn from its mistakes and adapt its strategy over time.
 *
 * Also since I didn't add a whole lot of comments while programming this, I was sort of lazy when adding additional comments
 * for the functions. I may go back and add more comments later, but for now, I just wanted to get this done.
 */