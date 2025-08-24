/*
 * @author: Anthony Ward
 * @upload date: 08/24/2025
 * 
 * 21 Blackjack implementation
 *
 * Features:
 *  - Multi-deck shoe using config.num_decks (clamped 1..8).
 *  - Shoe is shuffled on init and ONLY re-shuffled at the cut card
 *    (when >= CUT_CARD_PENETRATION_PERCENT% of the shoe has been dealt),
 *    or if not enough cards remain for the next operation.
 *
 * Notes:
 *  - Relies on deck.h's Card type and initialize_deck() helpers.
 *  - Uses global playerData, config, clear_screen(), save_player_data(),
 *    save_achievements(), checkAchievements(), blackjack() (menu), etc.
 */

#include "blackjack.h"

/* --------------------------------------------------------------------------- */
/* INTERNAL / FILE-LOCAL FUNCTIONS                                             */
/* --------------------------------------------------------------------------- */

/* Shoe lifecycle */
static void shoe_build(Shoe *shoe, int requestedDecks);
static void shoe_shuffle(Shoe *shoe);
static int  shoe_remaining(const Shoe *shoe);
static void shoe_ensure_cards(Shoe *shoe, int needed);  /* re-shuffle when low */

/* Gameplay helpers (file-local) */
static int  get_hand_value(Hand *hand);
static void print_hand(const char *name, Hand *hand);
static void deal_card(Shoe *shoe, Hand *hand);
static int  is_pair(Hand *hand);
static int  check_blackjack(Hand *hand);
static void play_hand(Shoe *shoe, Hand *hand, int roundNumber);

/* --------------------------------------------------------------------------- */
/* HOW TO PLAY / UI                                                            */
/* --------------------------------------------------------------------------- */

void blackjackHowToPlay(void)
{
    clear_screen();
    printf("=== HOW TO PLAY: 21 BLACKJACK ===\n");

    printf("\nObjective: Get a hand total as close to 21 without going over. Beat the dealer's hand to win.\n");

    printf("\nCard Values:\n");
    printf(" - Number Cards (2-10) = face value\n");
    printf(" - Face Cards (Jack, Queen, King) = 10 points\n");
    printf(" - Ace = 1 or 11 points (whichever is more favorable)\n");

    printf("\n---Gameplay Overview---\n");
    printf("Initial Deal:\n");
    printf(" - Each player and the dealer are dealt 2 cards.\n");
    printf(" - Players' cards are face up.\n");
    printf(" - Dealer shows one upcard; the other is face down (hole card).\n");

    printf("\nPlayer Actions:\n");
    printf(" - Hit: take another card\n");
    printf(" - Stand: stop taking cards\n");
    printf(" - Double Down: double your bet, take exactly one more card, and stand\n");
    printf(" - Split: if your two cards are a pair, split into two hands (new bet required)\n");
    printf(" - Surrender (optional): forfeit early and get half your bet back (first two cards only)\n");

    printf("\nDealer's Turn:\n");
    printf(" - Dealer plays after all players.\n");
    printf(" - Dealer reveals the hole card.\n");
    printf(" - Dealer hits until at least 17 (soft 17 hits per this ruleset).\n");

    printf("\nCompare Hands:\n");
    printf(" - Closer to 21 wins; tie is a push (bet returned).\n");
    printf(" - Natural Blackjack (Ace + 10-value on first two cards) beats other 21s.\n");

    printf("\n---Payouts---\n");
    printf("Win (normal)        = 1:1\n");
    printf("Blackjack (natural) = 3:2\n");
    printf("Insurance win       = 2:1\n");
    printf("Push (tie)          = bet returned\n");

    printf("\n---Insurance Bet---\n");
    printf("Offered when dealer's upcard is an Ace. You may place up to half your original bet.\n");

    printf("\n---Splitting Rules---\n");
    printf("Pairs may be split (up to 4 hands). Split Aces drawing a 10-value card do not count as Blackjack.\n");

    printf("\n---Doubling Down---\n");
    printf("After receiving your first two cards, you may double your bet, take one card, then stand.\n");

    printf("\n---Surrender---\n");
    printf("Allowed only on the first decision (not after a split).\n");

    pause_for_enter();
    clear_screen();
}

/* --------------------------------------------------------------------------- */
/* MAIN GAME LOOP                                                              */
/* --------------------------------------------------------------------------- */

void blackjack_start(void)
{
    /* Build and shuffle a shoe based on config.num_decks (clamped 1..8). */
    Shoe gameShoe;
    int requestedDecks = config.num_decks;
    if (requestedDecks < 1)                requestedDecks = 1;
    if (requestedDecks > MAX_SHOE_DECKS)   requestedDecks = MAX_SHOE_DECKS;

    srand((unsigned int)time(NULL));
    shoe_build(&gameShoe, requestedDecks);
    shoe_shuffle(&gameShoe);

    const unsigned int minBet = 10;
    const unsigned int maxBet = 500;

    int roundNumber = 1;

    while (playerData.uPlayerMoney >= minBet)
    {
        clear_screen();

        /* Ensure we can comfortably deal a round. Will reshuffle at the cut card
        or if fewer than `needed` cards remain. */
        shoe_ensure_cards(&gameShoe, 10);

        printf("=== Round %d ===\n", roundNumber);
        printf("You have: $%lld\n\n", playerData.uPlayerMoney);

        unsigned int betAmount = get_valid_bet(minBet, maxBet);

        Hand playerHand1 = { .count = 0, .bet = betAmount, .surrendered = 0, .doubled = false, .fromSplit = 0 };
        Hand playerHand2 = { .count = 0, .bet = 0, .surrendered = 0, .doubled = false, .fromSplit = 0 };
        Hand dealerHand  = { .count = 0, .bet = 0, .surrendered = 0, .doubled = false, .fromSplit = 0 };

        bool isSplit = false;

        playerData.uPlayerMoney -= betAmount;

        /* Initial deal: P, D, P, D */
        deal_card(&gameShoe, &playerHand1);
        deal_card(&gameShoe, &dealerHand);
        deal_card(&gameShoe, &playerHand1);
        deal_card(&gameShoe, &dealerHand);

        clear_screen();
        print_hand("Dealer shows", &(Hand){ .cards = { dealerHand.cards[0] }, .count = 1 });

        handle_insurance(&dealerHand);

        /* If dealer has Blackjack and player doesn't: immediate resolution. */
        if (check_blackjack(&dealerHand) && !check_blackjack(&playerHand1))
        {
            printf("Dealer has Blackjack. You lose this round.\n");
            resolve_hands(isSplit, &playerHand1, &playerHand2, &dealerHand);
            checkAchievements();
            save_player_data();
            save_achievements();
            if (!play_again()) break;
            roundNumber++;
            continue;
        }

        /* Player Blackjack handling (push if dealer also has it). */
        if (check_blackjack(&playerHand1))
        {
            handle_blackjack(&dealerHand, betAmount);
            roundNumber++;
            continue;
        }

        /* Offer split if first two cards are a pair. */
        if (is_pair(&playerHand1))
        {
            isSplit = handle_split(&gameShoe, &playerHand1, &playerHand2, betAmount);
            if (isSplit)
            {
                play_hand(&gameShoe, &playerHand1, roundNumber);
                play_hand(&gameShoe, &playerHand2, roundNumber);
                goto dealer_turn;
            }
        }

        play_hand(&gameShoe, &playerHand1, roundNumber);

    dealer_turn:
        /* If both hands surrendered, round ends. */
        if (playerHand1.surrendered && (!isSplit || playerHand2.surrendered)) {
            roundNumber++;
            continue;
        }

        /* If player busted (or both split hands busted), skip dealer play. */
        if ((!isSplit && get_hand_value(&playerHand1) > 21) ||
            (isSplit && get_hand_value(&playerHand1) > 21 && get_hand_value(&playerHand2) > 21))
        {
            resolve_hands(isSplit, &playerHand1, &playerHand2, &dealerHand);
            if (!play_again()) break;
            roundNumber++;
            continue;
        }

        dealer_play(&gameShoe, &dealerHand, roundNumber);
        resolve_hands(isSplit, &playerHand1, &playerHand2, &dealerHand);

        roundNumber++;

        if (playerData.uPlayerMoney < minBet) {
            printf("You don't have enough money to continue.\n");
            pause_for_enter();
            break;
        }
        if (!play_again()) break;
    }

    save_player_data();
    clear_screen();
}

/* ------------------------------------------------------------------------- */
/* INPUT / PROMPTS                                                           */
/* ------------------------------------------------------------------------- */

unsigned int get_valid_bet(unsigned int minBet, unsigned int maxBet)
{
    unsigned int betAmount = 0;

    do {
        unsigned long long maxAllowed = (playerData.uPlayerMoney < maxBet)
            ? playerData.uPlayerMoney
            : maxBet;

        printf("Enter your bet ($%u - $%llu): ", minBet, maxAllowed);
        scanf("%u", &betAmount);

        if (betAmount < minBet || betAmount > maxBet || betAmount > playerData.uPlayerMoney) {
            printf("Invalid bet. Please enter an amount between $%u and $%llu\n",
                   minBet, maxAllowed);
        }
    } while (betAmount < minBet || betAmount > maxBet || betAmount > playerData.uPlayerMoney);

    save_player_data();
    return betAmount;
}

/* ------------------------------------------------------------------------- */
/* INSURANCE / BLACKJACK / SPLIT                                             */
/* ------------------------------------------------------------------------- */

void handle_insurance(Hand *dealerHand)
{
    /* Simple fixed insurance bet (kept as-is from original). */
    unsigned int insuranceBet = 50;

    if (strcmp(dealerHand->cards[0].rank, "Ace") == 0)
    {
        int choice = 0;
        printf("Dealer shows Ace. Take insurance for $%u?\n", insuranceBet);
        printf("1: Yes\n");
        printf("2: No\n");
        printf("> ");
        scanf("%d", &choice);

        if (choice == 1)
        {
            if (check_blackjack(dealerHand))
            {
                printf("Dealer has Blackjack. Insurance pays 2:1 and you lose this round.\n");
                playerData.blackjack.insurance_success++;
                playerData.blackjack.losses++;
                playerData.uPlayerMoney += insuranceBet * 2;
                playerData.total_losses++;
                checkAchievements();

                /* End round if dealer has blackjack. */
                if (!play_again()) {
                    blackjack();
                    return;
                }
                return;
            }
            else
            {
                printf("Dealer does not have Blackjack. You lose insurance.\n");
                pause_for_enter();
                clear_screen();
                if (playerData.uPlayerMoney < insuranceBet) {
                    printf("Not enough money for insurance.\n");
                } else {
                    playerData.uPlayerMoney -= insuranceBet;
                }
                save_player_data();
            }
        }
    }
}

void handle_blackjack(Hand *dealerHand, unsigned int bet)
{
    if (check_blackjack(dealerHand))
    {
        printf("Both you and dealer have Blackjack. Push.\n");
        playerData.uPlayerMoney += bet;
        playerData.blackjack.draws++;
        playerData.total_draws++;
    }
    else
    {
        printf("Blackjack! You win 3:2.\n");
        playerData.blackjack.blackjack_wins++;
        playerData.uPlayerMoney += bet + (unsigned int)(bet * 1.5);
        playerData.blackjack.wins++;
        playerData.blackjack.win_streak++;
        if (playerData.blackjack.max_win_streak < playerData.blackjack.win_streak) {
            playerData.blackjack.max_win_streak = playerData.blackjack.win_streak;
        }
        playerData.total_wins++;
        checkAchievements();
    }

    save_player_data();

    if (!play_again()) {
        blackjack();
    }
}

bool handle_split(Shoe *shoe, Hand *playerHand1, Hand *playerHand2, unsigned int bet)
{
    int choice = 0;

    print_hand("Your hand", playerHand1);
    printf("\nYou have a pair. Split?\n");
    printf("1: Yes\n");
    printf("2: No\n");
    printf("> ");
    scanf("%d", &choice);

    if (choice == 1 && playerData.uPlayerMoney >= bet)
    {
        playerData.uPlayerMoney -= bet;

        playerHand2->cards[0]   = playerHand1->cards[1];
        playerHand2->count      = 1;
        playerHand2->bet        = bet;
        playerHand2->surrendered= 0;
        playerHand2->doubled    = false;
        playerHand2->fromSplit  = 1;

        playerHand1->count      = 1;
        playerHand1->fromSplit  = 1;

        deal_card(shoe, playerHand1);
        deal_card(shoe, playerHand2);
        return true;
    }
    else if (choice == 1)
    {
        printf("Not enough money to split.\n");
    }

    return false;
}

/* ------------------------------------------------------------------------- */
/* DEALER / RESOLUTION                                                       */
/* ------------------------------------------------------------------------- */

void dealer_play(Shoe *shoe, Hand *dealerHand, int roundNumber)
{
    clear_screen();
    printf("=== Round %d ===\n\nDealer's turn:\n", roundNumber);
    print_hand("Dealer", dealerHand);

    while (get_hand_value(dealerHand) < 17)
    {
        deal_card(shoe, dealerHand);
        print_hand("Dealer", dealerHand);
    }
}

void resolve_hands(bool isSplit, Hand *playerHand1, Hand *playerHand2, Hand *dealerHand)
{
    int dealerValue   = get_hand_value(dealerHand);
    int handsToResolve= isSplit ? 2 : 1;
    Hand *hands[]     = { playerHand1, playerHand2 };

    for (int i = 0; i < handsToResolve; ++i)
    {
        Hand *ph = hands[i];
        if (ph->count == 0 || ph->surrendered) continue;

        int playerValue = get_hand_value(ph);

        if (isSplit) printf("\nYour hand %d: ", i + 1);
        else         printf("\nYour hand: ");

        print_hand("", ph);
        printf("Your total: %d vs Dealer: %d\n", playerValue, dealerValue);

        if (playerValue > 21)
        {
            printf("You busted. Lose $%u\n", ph->bet);
            playerData.blackjack.losses++;
            playerData.blackjack.win_streak = 0;
            playerData.total_losses++;
        }
        else if (dealerValue > 21 || playerValue > dealerValue)
        {
            printf("You win! Gain $%u\n", ph->bet);
            playerData.uPlayerMoney += (ph->bet * 2);
            playerData.blackjack.wins++;
            playerData.blackjack.win_streak++;
            if (playerData.blackjack.max_win_streak < playerData.blackjack.win_streak) {
                playerData.blackjack.max_win_streak = playerData.blackjack.win_streak;
            }
            playerData.total_wins++;
            if (ph->doubled) playerData.blackjack.doubledown_wins++;
        }
        else if (playerValue < dealerValue)
        {
            printf("Dealer wins. Lose $%u\n", ph->bet);
            playerData.blackjack.losses++;
            playerData.blackjack.win_streak = 0;
            playerData.total_losses++;
        }
        else
        {
            printf("Push. No money gained or lost.\n");
            playerData.uPlayerMoney += ph->bet;
            playerData.blackjack.draws++;
            playerData.total_draws++;
        }
    }

    if (isSplit)
    {
        int win1 = (playerHand1->count > 0 && !playerHand1->surrendered &&
                    get_hand_value(playerHand1) <= 21 &&
                    (dealerValue > 21 || get_hand_value(playerHand1) > dealerValue));

        int win2 = (playerHand2->count > 0 && !playerHand2->surrendered &&
                    get_hand_value(playerHand2) <= 21 &&
                    (dealerValue > 21 || get_hand_value(playerHand2) > dealerValue));

        if (win1 && win2) playerData.blackjack.split_wins++;
    }

    playerData.games_played++;
    checkAchievements();
    save_player_data();
    save_achievements();
}

/* ------------------------------------------------------------------------- */
/* PLAY AGAIN                                                                */
/* ------------------------------------------------------------------------- */

bool play_again(void)
{
    int again = 0;
    printf("\nPlay another round?\n");
    printf("1: Yes\n");
    printf("2: No\n");
    printf("> ");
    scanf("%d", &again);
    clear_screen();
    return (again == 1);
}

/* ------------------------------------------------------------------------- */
/* HAND/DEAL UTILITIES                                                       */
/* ------------------------------------------------------------------------- */

/**
 * get_hand_value
 * Sum hand value with proper Ace adjustment (11 -> 1 as needed).
 */
static int get_hand_value(Hand *hand)
{
    int total = 0, aces = 0;

    for (int i = 0; i < hand->count; ++i)
    {
        if (strcmp(hand->cards[i].rank, "Ace") == 0) {
            total += 11; aces++;
        }
        else if (strcmp(hand->cards[i].rank, "King")  == 0 ||
                 strcmp(hand->cards[i].rank, "Queen") == 0 ||
                 strcmp(hand->cards[i].rank, "Jack")  == 0) {
            total += 10;
        }
        else {
            total += atoi(hand->cards[i].rank);
        }
    }

    while (total > 21 && aces > 0) {
        total -= 10; /* count one Ace as 1 instead of 11 */
        aces--;
    }

    return total;
}

/**
 * deal_card
 * Deal the next card from the shoe into a hand. If running low, the shoe
 * will be rebuilt/shuffled before dealing (via shoe_ensure_cards()).
 */
static void deal_card(Shoe *shoe, Hand *hand)
{
    if (hand->count >= MAX_HAND_CARDS) {
        printf("Hand is full!\n");
        return;
    }

    /* Ensure we have at least 1 card available; reshuffle when low. */
    shoe_ensure_cards(shoe, 1);

    if (shoe->next_index >= shoe->total) {
        printf("Shoe out of cards!\n");
        return;
    }

    hand->cards[hand->count++] = shoe->cards[shoe->next_index++];
}

/**
 * print_hand
 * Print cards in a hand with an optional label.
 */
static void print_hand(const char *name, Hand *hand)
{
    if (name && *name) printf("%s: ", name);
    for (int i = 0; i < hand->count; ++i) {
        printf("[%s of %s] ", hand->cards[i].rank, hand->cards[i].suit);
    }
    printf("\n");
}

/**
 * is_pair
 * True if the hand has exactly 2 cards of the same rank.
 */
static int is_pair(Hand *hand)
{
    return hand->count == 2 && strcmp(hand->cards[0].rank, hand->cards[1].rank) == 0;
}

/**
 * check_blackjack
 * True if exactly two cards sum to 21.
 */
static int check_blackjack(Hand *hand)
{
    return hand->count == 2 && get_hand_value(hand) == 21;
}

/**
 * play_hand
 * Drive player decisions for a single hand (Hit/Stand/Surrender/Double).
 */
static void play_hand(Shoe *shoe, Hand *hand, int roundNumber)
{
    int choice     = 0;
    int firstTurn  = 1;

    for (;;)
    {
        printf("=== Round %d ===\n\n", roundNumber);
        printf("-- Playing Your Hand --\n");
        print_hand("Your hand", hand);

        int currentTotal = get_hand_value(hand);
        printf("Current total: %d\n", currentTotal);

        if (currentTotal > 21) {
            break; /* bust */
        }

        printf("\n1: Hit\n");
        printf("2: Stand\n");
        if (firstTurn) {
            printf("3: Surrender (-50%%)\n");
            printf("4: Double Down\n");
        }
        printf("> ");
        scanf("%d", &choice);

        switch (choice)
        {
            case 1: /* Hit */
                deal_card(shoe, hand);
                firstTurn = 0;
                break;

            case 2: /* Stand */
                return;

            case 3: /* Surrender */
                if (!firstTurn || hand->fromSplit) {
                    printf("Surrender is only allowed at the start and not after a split.\n");
                    break;
                }
                printf("You surrendered. Lose half your bet.\n");
                hand->surrendered = 1;
                playerData.uPlayerMoney += hand->bet / 2;
                hand->count = 0; /* remove cards for clarity */
                playerData.blackjack.win_streak = 0;

                if (!play_again()) {
                    blackjack();
                }
                return;

            case 4: /* Double Down */
                if (playerData.uPlayerMoney < hand->bet) {
                    printf("Not enough money to double down.\n");
                    break;
                }
                printf("Doubling down.\n");
                playerData.uPlayerMoney -= hand->bet;
                hand->bet *= 2;
                hand->doubled = true;
                deal_card(shoe, hand);
                print_hand("Your hand after double down", hand);
                return;

            default:
                clear_screen();
                break;
        }

        clear_screen();
    }
}

/* ------------------------------------------------------------------------- */
/* SHOE IMPLEMENTATION                                                       */
/* ------------------------------------------------------------------------- */

/**
 * shoe_build
 * Fill the shoe with N concatenated decks (each via initialize_deck()).
 */
static void shoe_build(Shoe *shoe, int requestedDecks)
{
    if (!shoe) return;

    if (requestedDecks < 1)              requestedDecks = 1;
    if (requestedDecks > MAX_SHOE_DECKS) requestedDecks = MAX_SHOE_DECKS;

    shoe->decks_in_shoe = requestedDecks;
    shoe->total         = requestedDecks * DECK_SIZE;
    shoe->next_index    = 0;

    /* Build by appending initialized decks. */
    Card tempDeck[DECK_SIZE];

    int writePos = 0;
    for (int d = 0; d < requestedDecks; ++d)
    {
        initialize_deck(tempDeck);
        /* keep .revealed and string fields exactly as deck.h sets them */

        for (int i = 0; i < DECK_SIZE; ++i) {
            shoe->cards[writePos++] = tempDeck[i];
        }
    }
}

/**
 * shoe_shuffle
 * Fisher-Yates shuffle over the entire shoe.
 */
static void shoe_shuffle(Shoe *shoe)
{
    if (!shoe) return;

    for (int i = shoe->total - 1; i > 0; --i)
    {
        int j = rand() % (i + 1);
        Card tmp       = shoe->cards[i];
        shoe->cards[i] = shoe->cards[j];
        shoe->cards[j] = tmp;
    }

    shoe->next_index = 0;
}

/**
 * shoe_remaining
 * Count of undealt cards in the shoe.
 */
static int shoe_remaining(const Shoe *shoe)
{
    return (shoe && shoe->total >= shoe->next_index)
         ? (shoe->total - shoe->next_index)
         : 0;
}

/**
 * shoe_ensure_cards
 * If remaining cards < SHOE_RESHUFFLE_THRESHOLD or < needed,
 * rebuild and shuffle the shoe (same number of decks).
 */
static void shoe_ensure_cards(Shoe *shoe, int needed)
{
    if (!shoe) return;

    const int remaining = shoe_remaining(shoe);

    /* Compute cut index (number of cards dealt at which we reshuffle). */
    int pct = CUT_CARD_PENETRATION_PERCENT;
    if (pct < 50) pct = 50;           /* sanity clamp to sensible range */
    if (pct > 95) pct = 95;

    const int cut_index = (shoe->total * pct) / 100;       /* cards dealt */
    const int dealt     = shoe->next_index;

    if (remaining < needed || dealt >= cut_index)
    {
        const int decks = shoe->decks_in_shoe;
        shoe_build(shoe, decks);
        shoe_shuffle(shoe);
    }
}
