# Deck of Card Simulation - v1.4.0
**Release Date:** 2025-08-24  

A program that simulates realistic card game logic with multiple playable games.  

## Added
- **Achievements**: Unlockable achievements for each supported game.  
- **Autosave**: Games now save progress automatically.  
- **Multiple Save Slots**: Added support for multiple save slots in **Solitaire**.  
- **Winnable Solitaire Deals**: Implemented a Depth First Search (DFS) system with transposition tables, move ordering, pruning heuristics, and forced move passes to ensure solvable Solitaire boards.  
- **Player Data Persistence**: Tracks and saves progress and features across sessions.  
- **Other Menu**: New menu with options to change game rules, view stats, and reset player data.  

## Changed
- Reorganized project folder structure for clarity.  
- Reformatted the entire codebase for consistency.  
- Updated commenting style to improve readability and maintainability.  
- Updated **Idiot** bot difficulty
- Update deck handling in **21 Blackjack**

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
- Local and network multiplayer support (*).  
- GUI implementation using OpenGL (*).  
- Machine learning AI for computer opponents (*).

**Full Changelog**: https://github.com/award20/Card-Simulation/compare/v1.3.0...v1.4.0
