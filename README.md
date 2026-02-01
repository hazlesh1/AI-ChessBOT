# Bitboard Chess Engine
Developer: Leo Girard (14, Feb 2026)

Location: Den Haag, NL

Started: February 2026

## Project Overview
This is a high-performance chess engine written in C using bitboard representation. This project uses 64-bit integers to allow for faster move generation and position analysis.

## Unique Features
Minimax Search with Alpha-Beta Pruning: The engine uses a custom search algorithm that calculates the best moves by exploring millions of future possibilities while pruning inefficient branches to save time.

Grandmaster Opening Database: For the first 5 moves of a game, the AI utilizes a database of real Grandmaster games to ensure a strong theoretical start.

Live Position Evaluation: The engine provides a real-time score to show which side is winning based on material balance and positional advantages.

Adjustable Difficulty Levels: Implementation of different AI strengths by scaling the search depth and introducing human-like move variance for lower levels.

## Documentation
I maintain a comprehensive Developer's Journal and Technical Specification. If you are reviewing this for a university application or technical audit, please navigate to the documentation folder:

docs/docs.md

This documentation includes the 6-week project roadmap, bitboard theory, and logs regarding the transition from Python-based physics simulations to low-level C optimization.
