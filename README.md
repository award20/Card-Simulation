# Deck of Card Simulation - v1.3.0
**Release Date:** 08/11/2025

A simple program that simulates real card logic with some applicable use cases.

## Added
- Implemented full **Idiot** card game.
- Added support for saving and loading a game of **Solitaire**, with overwrite capability.
- Added an **Auto Complete** feature to solitaire.

## Changed
- Reformatted project structure to use cleaner data separation:
  - Moved structs and function prototypes into dedicated header files.
- Made user selection prompts consistent across all games.
- Removed some "Magic numbers"

## Currently Supported Games
- 21 Blackjack
- Klondike Solitaire
- Idiot

## Planned Games
- Texas Hold'em
- 5-Card Poker
- Rummy
- Go Fish
- War
- Spades
- Crazy Eights

## Planned Features
_* = tentative_
- Support for local multiplayer (pass the keyboard)
- Statistics tracking (wins, losses, streaks for each game)
- Multiple save slots per game type
- Customizable rules (e.g. number of decks, Jokers on/off, etc)
- Ensure winnable solitaire deals (*)
- LAN/Network play (*)
- GUI implementation using OpenGL (*)
- Machine learning AI for computer opponents (*)

**Full Changelog**: https://github.com/award20/Card-Simulation/compare/v1.2.0...v1.3.0
