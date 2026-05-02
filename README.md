# AI-ChessBOT

**Source Code:** https://github.com/hazlesh1/AI-ChessBOT

**Date:** 2026/02/01 (start) - 2026/04/29 (end)

**Developer:** Leo Girard, (14)

**Location:** Netherlands Den Haag

---

## Instructions

Navigate into Source-Code

```plaintext
ディレクトリ: C:XXX

Mode                LastWriteTime          Length Name
----                -------------          ------ ----
d-----         2026/03/29     16:23                templates
d-----         2026/04/28      1:56                venv
-a----         2026/05/02      7:00           17180 draft.c
-a----         2026/05/02      7:04           33558 main.c
-a----         2026/05/02      7:00          164633 main.exe
-a----         2026/05/02      6:59             796 server.py
```

This should be included. Run server.py using python to test the engine and play against the engine.

`main.c` includes the main code for the engine.

Feel free to edit the `index.html` in the templates directory to make the game more enjoyable.

I recommend taking a look at `draft.c`, as it includes lots of comments and me crashing out live on my code (even though it only includes half the code in `main.c`).

> "A bitboard chess engine in C using Alpha-Beta Pruning and Negamax"

---

## Developer

I am Leo Girard; a 14 years old student in the Netherlands at the time (2026/04/29). Proudly passed the CS50x Introduction to computer science while doing this project.

---

## Introduction: Why C?

First of all, you might be wondering: Why C?

Chess engines are indeed simple to make in python with arrays. However, the complexity and millions of possibilities means that the engine must be heavily optimized. Chess has 10^120 possible games ("Shannon Number"). It is unrealistic to calculate each possible game, even with the most powerful machines.

I chose C for manual memory management and bitwise operations O(1), which can be used to heavily optimize the engine.

The goal of this project is to make a working chess engine a player can interact with (mostly backend). More specifically, this includes a full-stack engine:

- Low-level C logic
- Python Middleware
- JavaScript Frontend

This project is heavily aiming at the C logic.

---

## Data Architecture: Using the full potential of 64-Bits

In C, `uint64_t` is exactly 64 bits. A chess board has 8 rows and 8 columns, so 64 squares. Notice the matching. We can use this advantage to store a whole chess board into 64-Bits.

This is where bitboards come in. Bitboards are highly efficient data structures (Wikipedia Contributors).

Specifically here, our `Board` struct includes 12 bitboards, occupancies, history and some integers to support game status. (`GameState` includes 2 ints):

```c
typedef struct {
    uint64_t bitboards[12];
    int side;  
    uint64_t occupancies[3];
    int enpassant;          
    int castling;
    GameState history[1024];
    int history_ptr;          
} Board; 
```

- Each of the 12 bitboards represents a specific piece type (6 per side). This allows efficient access and move generation.
- Each bitboard uses individual bits to represent a piece on a square (1 = occupied, 0 = empty).
- Despite multiple bitboards, occupancies, and game state history, the entire structure only takes about 8328 bytes, which is extremely compact for a full chess engine state.

For comparison, a naive Python implementation that stores full board states as integer arrays for each history entry would require approximately 1.8–2.0 MB of memory. However, actual memory usage depends heavily on the implementation details. C remains significantly more memory-efficient for systems like this due to its ability to use fixed-size, low-overhead data structures.

---

## Bitwise Optimization

In Python, move generation is often done using loops that check each square individually. For example, a rook would scan squares one by one in each direction until it reaches the edge of the board or another piece.

This overall means that the program must evaluate multiple squares multiple times, which adds computational overhead, especially when this process is repeated thousands and millions of times during search.

In C, the entire board is shown as a single 64-bit integer, where each bit corresponds to a square using bitboards. The engine uses bitwise operations like AND, OR and bit shifts to change and evaluate multiple pieces through a single value.

Because the full board is encoded in one 64-bit number, these operations allow the engine to work with many squares at once (while staying efficient), rather than iteration one by one.

---

## Negamax and Alpha-beta pruning

The search algorithm mainly uses these two: Alpha-Beta Pruning and Negamax.

**Alpha-Beta Pruning** is a search algorithm that reduces the number of nodes evaluated in the search tree.

In my Code:

```c
if (score >= beta)
    return beta;

if (score > alpha)
    alpha = score;
```

- The first part is the beta cutoff. If a move is "too" good, the opponent won't allow it (assuming the opponent plays perfect).
- The second part is updating alpha for a best move for the current side.

More specifically, alpha is the lower bound or the best score we can get.

Beta is the upper bound or the best score that the opponent will allow us to get.

**Negamax** is an algorithm that simplifies code by treating every position from the perspective of the player whose turn it is.

In my Code:

```c
int score = -alpha_beta(board, -beta, -alpha, depth - 1);
```

Simply, this is a change in perspective. The "best" score for you is the worst score for the opponent (thus, negative) and vice versa.

Overall, the Shannon number makes it computationally near impossible to search every branch. Alpha-Beta pruning works by cutting off branches that are mathematically proven to be worse than a previously discovered move. Negamax is a simple algorithm that makes it simpler to change perspectives and reduces the amount of times the scores are calculated.

---

## Moving in more deeply in the code

### FEN (Forsyth-Edwards Notation)

FEN looks something like this: `"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"`

It is used to represent a whole board into a simple 1 line string. The different letters represent different pieces. The integers represent blank pieces on the board. At the end, simple game status such as the player's turn, castling status, etc. is stored.

This ensures we can handle the game without having to throw a whole board structure between the frontend and backend.

### Move Generation: Lookup tables and on the fly

A main part of chess engine optimization is deciding when to use Memory and CPU. This engine is hybrid.

For **leaper Pieces**, pre-calculated lookup tables are used. The pieces have the same sliding paths and cannot be blocked, meaning their attack patterns from any square are constant. This is why they are calculated at the start. During the search, we can then perform a single memory fetch.

For **sliding pieces**, we use On-The-Fly. Unlike leapers, a rook's path can vary based on other pieces on the board.

```c
else if (piece == B || piece == b)
    attacks = mask_bishop_attacks(source_square, board->occupancies[BOTH]);

else if (piece == R || piece == r)
    attacks = mask_rook_attacks(source_square, board->occupancies[BOTH]);

else
    attacks = mask_queen_attacks(source_square, board->occupancies[BOTH]);
```

> Note: I am aware of magic bitboards and that they would allow sliding attacks to become O(1). However, due to the length of the project, my age and understanding of code, I have not implemented this.

### PSTs (Piece-Square Tables)

Raw calculations aren't enough for high-level play. This is where PSTs come in to give engine positional intuitions.

For example, a Knight in the center of the board is objectively more powerful than a Knight in the corner. It is encoded into 64-integer arrays.

Example for knights:

```c
const int knight_pst[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};
```

Knights often are more "useful" and are proven to play better when they are near the center. These scores make sure the moves aren't purely based on raw calculations, but on the location too.

### Make and Unmake

A challenging part of the C backend was managing the board state during the recursive search. Instead of copying the entire `Board` struct millions of times (which isn't efficient), we use a destructive move/undo system. This uses the `GameState` history stack to rewind the board after a branch has been searched.

- **Make move** is used to actually play a move during the search.
- **Unmake move** is used to undo that move and restore the original board.

When calling `make_move()`, the engine first saves important states such as castling rights, en passant squares and captured pieces. Then, it updates the board by moving pieces, removing pieces, updating sides, etc.
When calling `unmake_move()`, the engine restores saved data that `make_move()` stored in the history `GameState` struct. Then, it reverts all changes made by `make_move()`.

```c
board->history[board->history_ptr].castling = board->castling;
board->history[board->history_ptr].enpassant = board->enpassant;
board->history_ptr++;
```

*(How `make_move()` stores the important data such as enpassant and castling)*

---

## Bottlenecks and goals for future similar projects

Right now, the engine has a horizon effect. If a capture happens just beyond the depth limit, the engine might make a suicidal move because it can't see one step further…

Plausible solutions for next projects:

- Implementing **quiescence search** to continue searching "loud" positions (captures/checks) until the board becomes "quiet."
- **Transposition tables** where chess paths lead to the same position…
- Using **Zobrist Hashing** to store previously evaluated positions in a hash map, preventing redundant calculations.

I also am personally interested in magic bitboards and would be invested in exploring it.

Overall, these are some future goals that I would implement if I ever get to code a similar project. I fully appreciate people who took their time to read this fully.

Thanks!