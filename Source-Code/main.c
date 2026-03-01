#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Date: 2026/02/21
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 4 – Occupancy & Sliding Piece Attacks (Rook, Bishop, Queen)

// ================================
// MACROS (inline bit helpers)
// ================================
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

// ================================
// ENUMS (Piece & Side IDs)
// ================================
enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };

// ================================
// BOARD STRUCT (Game State)
// ================================
typedef struct {
    uint64_t bitboards[12];
    int side;
} Board;

// ================================
// FEN HELPERS & PARSER
// ================================
int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1;
}

void parse_fen(char *fen, Board *board) {
    memset(board, 0, sizeof(Board));
    int square = 0;
    while (*fen != ' ') {
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
            int piece = char_to_piece(*fen);
            set_bit(board->bitboards[piece], square);
            square++;
        } else if (*fen >= '1' && *fen <= '8') {
            square += (*fen - '0');
        }
        fen++;
    }
    fen++;
    board->side = (*fen == 'w') ? WHITE : BLACK;
}

// ================================
// LEAPER ATTACK TABLES (Week 3)
// ================================
uint64_t knight_attacks[64];
uint64_t king_attacks[64];

uint64_t mask_knight_attacks(int square) {
    uint64_t attacks = 0ULL;
    int rank = square / 8, file = square % 8;
    int r_off[] = {-2, -2, -1, -1, 1, 1, 2, 2};
    int c_off[] = {-1, 1, -2, 2, -2, 2, -1, 1};
    for (int i = 0; i < 8; i++) {
        int tr = rank + r_off[i], tf = file + c_off[i];
        if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8) set_bit(attacks, tr * 8 + tf);
    }
    return attacks;
}

uint64_t mask_king_attacks(int square) {
    uint64_t attacks = 0ULL;
    int rank = square / 8, file = square % 8;
    int r_off[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int c_off[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    for (int i = 0; i < 8; i++) {
        int tr = rank + r_off[i], tf = file + c_off[i];
        if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8) set_bit(attacks, tr * 8 + tf);
    }
    return attacks;
}

// ================================
// WEEK 4 – SLIDING PIECE PHYSICS
// ================================

// Combines all 12 bitboards to see the "Floor"
uint64_t get_occupancy(Board *board) {
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < 12; i++) occupancy |= board->bitboards[i];
    return occupancy;
}

// Bishop Ray-Casting: Diagonal movement
uint64_t mask_bishop_attacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    int r, f;

    // Top-Right, Top-Left, Bottom-Right, Bottom-Left
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(occupancy, r * 8 + f)) break; // Hit a piece! Stop the ray.
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(occupancy, r * 8 + f)) break;
    }
    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(occupancy, r * 8 + f)) break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(occupancy, r * 8 + f)) break;
    }
    return attacks;
}

// Rook Ray-Casting: Orthogonal movement
uint64_t mask_rook_attacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    int r, f;

    for (r = tr - 1; r >= 0; r--) { // Up
        set_bit(attacks, r * 8 + tf);
        if (get_bit(occupancy, r * 8 + tf)) break;
    }
    for (r = tr + 1; r <= 7; r++) { // Down
        set_bit(attacks, r * 8 + tf);
        if (get_bit(occupancy, r * 8 + tf)) break;
    }
    for (f = tf - 1; f >= 0; f--) { // Left
        set_bit(attacks, tr * 8 + f);
        if (get_bit(occupancy, tr * 8 + f)) break;
    }
    for (f = tf + 1; f <= 7; f++) { // Right
        set_bit(attacks, tr * 8 + f);
        if (get_bit(occupancy, tr * 8 + f)) break;
    }
    return attacks;
}

// Queen is just Rook + Bishop combined
uint64_t mask_queen_attacks(int square, uint64_t occupancy) {
    return mask_rook_attacks(square, occupancy) | mask_bishop_attacks(square, occupancy);
}

// ================================
// INITIALIZATION & DEBUG
// ================================
void init_all() {
    for (int i = 0; i < 64; i++) {
        knight_attacks[i] = mask_knight_attacks(i);
        king_attacks[i] = mask_king_attacks(i);
    }
}

void print_bitboard(uint64_t bitboard) {
    printf("\n");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (!col) printf(" %d ", 8 - row);
            printf(" %d ", get_bit(bitboard, row * 8 + col));
        }
        printf("\n");
    }
    printf("\n     a  b  c  d  e  f  g  h\n\n");
}

// ================================
// MAIN – Week 4 Collision Test
// ================================
int main() {
    init_all();
    Board board;

    // FEN with a Rook on e4 and a Pawn blocking it at e6
    char *test_fen = "8/8/4p3/8/4R3/8/8/8 w - - 0 1";
    parse_fen(test_fen, &board);
    
    uint64_t occ = get_occupancy(&board);
    
    printf("--- WEEK 4: COLLISION TEST ---\n");
    printf("Rook on e4, Blocked by piece on e6:\n");
    
    // Generate attacks for the Rook at e4 (index 36)
    uint64_t rook_atk = mask_rook_attacks(36, occ);
    print_bitboard(rook_atk);

    return 0;
}