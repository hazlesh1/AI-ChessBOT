#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Date: 2026/02/21
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 4 – Occupancy & Sliding Piece Attacks (Rook, Bishop, Queen)

// ================================================================
// 1. MACROS & CONSTANTS
// ================================================================

// BIT MANIPULATION HELPERS
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)  
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

// MOVE ENCODING MACROS (32-bit integer)
// source(6) | target(6) | piece(4) | promoted(4) | capture(1) | double_push(1) | enpassant(1) | castling(1)
#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((double_push) << 21) | ((enpassant) << 22) | ((castling) << 23))

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) >> 6) & 0x3f)

// ENUMS (Pieces and Sides as numbers)
enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };

// ================================================================
// 2. DATA STRUCTURES
// ================================================================

// MOVE LIST STRUCT
typedef struct {
    uint32_t moves[256];
    int count;
} MoveList;

// BOARD STRUCT (Game State)
typedef struct { 
    uint64_t bitboards[12]; 
    int side;  
    uint64_t occupancies[3]; // [WHITE], [BLACK], [BOTH]
    int enpassant;           
    int castling;            
} Board;

// ================================================================
// 3. GLOBAL LOOKUP TABLES
// ================================================================

uint64_t knight_attacks[64];
uint64_t king_attacks[64];
uint64_t pawn_attacks[2][64]; 

// ================================================================
// 4. LOW-LEVEL PHYSICS
// ================================================================



uint64_t mask_pawn_attacks(int side, int square) {
    uint64_t attacks = 0ULL;
    uint64_t piece_bitboard = 0ULL;
    set_bit(piece_bitboard, square);

    if (side == WHITE) {
        if ((piece_bitboard >> 7) & 0xfefefefefefefefeULL) attacks |= (piece_bitboard >> 7);
        if ((piece_bitboard >> 9) & 0x7f7f7f7f7f7f7f7fULL) attacks |= (piece_bitboard >> 9);
    } else {
        if ((piece_bitboard << 7) & 0x7f7f7f7f7f7f7f7fULL) attacks |= (piece_bitboard << 7);
        if ((piece_bitboard << 9) & 0xfefefefefefefefeULL) attacks |= (piece_bitboard << 9);
    }
    return attacks;
}

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

uint64_t mask_bishop_attacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    int r, f;
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        set_bit(attacks, r * 8 + f);
        if (get_bit(occupancy, r * 8 + f)) break; 
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

uint64_t mask_rook_attacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    int r, f;
    for (r = tr - 1; r >= 0; r--) { 
        set_bit(attacks, r * 8 + tf);
        if (get_bit(occupancy, r * 8 + tf)) break;
    }
    for (r = tr + 1; r <= 7; r++) { 
        set_bit(attacks, r * 8 + tf);
        if (get_bit(occupancy, r * 8 + tf)) break;
    }
    for (f = tf - 1; f >= 0; f--) { 
        set_bit(attacks, tr * 8 + f);
        if (get_bit(occupancy, tr * 8 + f)) break;
    }
    for (f = tf + 1; f <= 7; f++) { 
        set_bit(attacks, tr * 8 + f);
        if (get_bit(occupancy, tr * 8 + f)) break;
    }
    return attacks;
}

uint64_t mask_queen_attacks(int square, uint64_t occupancy) {
    return mask_rook_attacks(square, occupancy) | mask_bishop_attacks(square, occupancy);
}

// ================================================================
// 5. HELPER LOGIC
// ================================================================

uint64_t get_occupancy(Board *board) { 
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < 12; i++) occupancy |= board->bitboards[i];
    return occupancy;
}

static inline void add_move(MoveList *move_list, uint32_t move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1;
}

void parse_fen(char *fen, Board *board) { 
    memset(board, 0, sizeof(Board)); 
    board->enpassant = -1; 
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
    
    for (int p = P; p <= K; p++) board->occupancies[WHITE] |= board->bitboards[p];
    for (int p = p; p <= k; p++) board->occupancies[BLACK] |= board->bitboards[p];
    board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// ================================================================
// 6. HIGH-LEVEL SYSTEMS (Move Gen & Initialization)
// ================================================================

void generate_moves(Board *board, MoveList *move_list) {
    move_list->count = 0;
    int source_square, target_square;
    uint64_t bitboard, attacks;

    // Example 1: Generate Knight Moves
    int knight_piece = (board->side == WHITE) ? N : n;
    bitboard = board->bitboards[knight_piece];
    while (bitboard) {
        source_square = __builtin_ctzll(bitboard); 
        attacks = knight_attacks[source_square] & ~board->occupancies[board->side];
        while (attacks) {
            target_square = __builtin_ctzll(attacks);
            int capture_flag = get_bit(board->occupancies[(board->side == WHITE) ? BLACK : WHITE], target_square);
            add_move(move_list, encode_move(source_square, target_square, knight_piece, 0, capture_flag, 0, 0, 0));
            attacks &= attacks - 1; 
        }
        bitboard &= bitboard - 1; 
    }

    // Example 2: Generate Rook Moves (Sliders)
    int rook_piece = (board->side == WHITE) ? R : r;
    bitboard = board->bitboards[rook_piece];
    while (bitboard) {
        source_square = __builtin_ctzll(bitboard); 
        attacks = mask_rook_attacks(source_square, board->occupancies[BOTH]) & ~board->occupancies[board->side];
        while (attacks) {
            target_square = __builtin_ctzll(attacks);
            int capture_flag = get_bit(board->occupancies[(board->side == WHITE) ? BLACK : WHITE], target_square);
            add_move(move_list, encode_move(source_square, target_square, rook_piece, 0, capture_flag, 0, 0, 0));
            attacks &= attacks - 1;
        }
        bitboard &= bitboard - 1;
    }
}

void init_all() { 
    for (int i = 0; i < 64; i++) { 
        pawn_attacks[WHITE][i] = mask_pawn_attacks(WHITE, i);
        pawn_attacks[BLACK][i] = mask_pawn_attacks(BLACK, i);
        knight_attacks[i] = mask_knight_attacks(i);
        king_attacks[i] = mask_king_attacks(i);
    }
}

// ================================================================
// 7. DEBUG & UI (Human-readable output)
// ================================================================

void print_bitboard(uint64_t bitboard) {
    printf("\n");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (!col) printf(" %d ", 8 - row);
            printf(" %d ", get_bit(bitboard, row * 8 + col));
        }
        printf("\n");
    }
    printf("\n    a  b  c  d  e  f  g  h\n\n");
}

// ================================================================
// 8. MAIN ENTRY POINT
// ================================================================

int main() {
    init_all();
    Board board;

    char *test_fen = "8/8/4p3/8/4R3/8/8/8 w - - 0 1";
    parse_fen(test_fen, &board);
    
    printf("--- WEEK 5: MOVE GENERATION TEST ---\n");
    MoveList moves;
    generate_moves(&board, &moves);
    printf("Total pseudo-legal moves for Knights and Rooks generated: %d\n", moves.count);

    return 0;
}