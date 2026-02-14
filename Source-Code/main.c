#include <stdio.h>
#include <stdint.h>
#include <string.h> // for memset

// Date: 2026/02/14
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 3 – Leaper Attack Generation (Knight + King)


// ================================
// MACROS (inline bit helpers)
// ================================

// Returns 1 if a bit is set on a square, otherwise 0
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)

// Sets a bit on a square
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))


// ================================
// ENUMS (Piece & Side IDs)
// ================================

// Piece indices (White = uppercase, Black = lowercase)
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

// Side to move
enum { WHITE, BLACK, BOTH };


// ================================
// BOARD STRUCT (Game State)
// ================================

typedef struct {
    uint64_t bitboards[12]; // One bitboard per piece type
    int side;               // Side to move
} Board;


// ================================
// FEN Helpers
// ================================

// Converts a FEN character into internal piece ID
int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1; // Invalid piece
}


// ================================
// FEN Parser
// ================================

// Loads board position from a FEN string
void parse_fen(char *fen, Board *board) {

    // Clear board state
    memset(board, 0, sizeof(Board));

    int square = 0; // Start at A8 (index 0)

    // Parse piece placement section
    while (*fen != ' ') {

        // If it's a piece letter
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
            int piece = char_to_piece(*fen);
            set_bit(board->bitboards[piece], square);
            square++;
        }
        // If it's a number (empty squares)
        else if (*fen >= '1' && *fen <= '8') {
            square += (*fen - '0');
        }

        // Skip '/'
        fen++;
    }

    // Move to side-to-move section
    fen++;
    board->side = (*fen == 'w') ? WHITE : BLACK;
}


// ================================
// WEEK 3 – Leaper Attack Tables
// ================================

// Precomputed attack maps
uint64_t knight_attacks[64];
uint64_t king_attacks[64];


// ================================
// Knight Attack Generator
// ================================

// Returns attack bitboard for a knight on given square
uint64_t mask_knight_attacks(int square) {

    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // All 8 knight offsets
    int row_offsets[] = {-2, -2, -1, -1,  1,  1,  2,  2};
    int col_offsets[] = {-1,  1, -2,  2, -2,  2, -1,  1};

    for (int i = 0; i < 8; i++) {

        int target_rank = rank + row_offsets[i];
        int target_file = file + col_offsets[i];

        // Stay inside board
        if (target_rank >= 0 && target_rank < 8 &&
            target_file >= 0 && target_file < 8) {

            int target_square = target_rank * 8 + target_file;
            set_bit(attacks, target_square);
        }
    }

    return attacks;
}


// ================================
// King Attack Generator
// ================================

// Returns attack bitboard for a king on given square
uint64_t mask_king_attacks(int square) {

    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // All 8 king directions
    int row_offsets[] = {-1, -1, -1,  0,  0,  1,  1,  1};
    int col_offsets[] = {-1,  0,  1, -1,  1, -1,  0,  1};

    for (int i = 0; i < 8; i++) {

        int target_rank = rank + row_offsets[i];
        int target_file = file + col_offsets[i];

        if (target_rank >= 0 && target_rank < 8 &&
            target_file >= 0 && target_file < 8) {

            int target_square = target_rank * 8 + target_file;
            set_bit(attacks, target_square);
        }
    }

    return attacks;
}


// ================================
// Initialization
// ================================

// Precompute all knight and king attacks
void init_leapers() {

    for (int square = 0; square < 64; square++) {
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square]   = mask_king_attacks(square);
    }
}


// ================================
// Debug: Print Bitboard
// ================================

// Prints bitboard as 8x8 grid
void print_bitboard(uint64_t bitboard) {

    printf("\n");

    for (int row = 0; row < 8; row++) {

        for (int col = 0; col < 8; col++) {

            int square = row * 8 + col;

            if (!col) printf(" %d ", 8 - row);

            printf(" %d ", get_bit(bitboard, square));
        }

        printf("\n");
    }

    printf("\n     a  b  c  d  e  f  g  h\n\n");
    printf("     Bitboard Value (Decimal): %llu\n\n", bitboard);
}


// ================================
// MAIN – Week 3 Test
// ================================

int main(){

    // Initialize attack tables
    init_leapers();

    printf("--- WEEK 3: LEAPER ATTACK TEST ---\n");

    // Test 1: Knight on e4 (index 36)
    int knight_square = 36;
    printf("Knight attacks from e4:\n");
    print_bitboard(knight_attacks[knight_square]);

    // Test 2: King on e8 (index 4)
    int king_square = 4;
    printf("King attacks from e8:\n");
    print_bitboard(king_attacks[king_square]);

    return 0;
}
