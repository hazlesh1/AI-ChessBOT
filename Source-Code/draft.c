#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Date: 2026/02/21
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 4 – Occupancy & Sliding Piece Attacks (Rook, Bishop, Queen)
// Week 6 – Completing the Move Generator & Legality
// Instead of an O(n) loop checking 64 squares to find a piece, my bitboards allow for O(1) lookup. I'm performing 64 calculations in a single CPU cycle using bitwise operators
// Some notes: AND -> both, OR -> either, XOR -> different, NOT -> flip, PARALLEL bitboard, 12 layers for more efficiency 

// ----------------------------------------------------------------
// MACROS & CONSTANTS
// ----------------------------------------------------------------
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define clear_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((capture) << 20) | ((double_push) << 21) | ((enpassant) << 22) | ((castling) << 23))

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) >> 6) & 0x3f)

enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };

// ----------------------------------------------------------------
// DATA STRUCTURES
// ----------------------------------------------------------------
// == WEEK 5 ==
// MOVE LIST STRUCT
typedef struct {
    uint32_t moves[256];
    int count;
} MoveList;

static inline void add_move(MoveList *move_list, uint32_t move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}
// ============

// BOARD STRUCT (Game State)
typedef struct { // New data type name structure 
    uint64_t bitboards[12]; // Array of 12 unsigned 64 bit integers (6x2 piece types), each uint is 8 bytes meaning equals to 96 bytes 
    int side;  // turn indicator 
    uint64_t occupancies[3]; // [WHITE], [BLACK], [BOTH]
    int enpassant;           // Square index for en passant (or -1)
    int castling;            // 4-bit integer tracking castling rights
} Board;
// This whole thing can be visualized by LAYERS, so basically each types of pieces have their own bitboards, making it efficient. 

// ----------------------------------------------------------------
// GLOBAL ATTACK TABLES
// ----------------------------------------------------------------
// Array of 64 bitboards -> calculating every moves a piece can take, looks at a position then calculates. Basically a lookup table.
uint64_t knight_attacks[64];
uint64_t king_attacks[64];
uint64_t pawn_attacks[2][64]; // [side][square]

// ----------------------------------------------------------------
// LEAPER ATTACK MASKING (Physics)
// ----------------------------------------------------------------
// == WEEK 5 ==
uint64_t mask_pawn_attacks(int side, int square) {
    uint64_t attacks = 0ULL;
    uint64_t piece_bitboard = 0ULL;
    set_bit(piece_bitboard, square);

    // Prevent pieces from wrapping around the board edges during capture
    if (side == WHITE) {
        if ((piece_bitboard >> 7) & 0xfefefefefefefefeULL) attacks |= (piece_bitboard >> 7);
        if ((piece_bitboard >> 9) & 0x7f7f7f7f7f7f7f7fULL) attacks |= (piece_bitboard >> 9);
    } else {
        if ((piece_bitboard << 7) & 0x7f7f7f7f7f7f7f7fULL) attacks |= (piece_bitboard << 7);
        if ((piece_bitboard << 9) & 0xfefefefefefefefeULL) attacks |= (piece_bitboard << 9);
    }
    return attacks;
}
// ============

uint64_t mask_knight_attacks(int square) {
    uint64_t attacks = 0ULL;
    int rank = square / 8, file = square % 8; // Converting single number back into a 2D grid w R rank and C file. % 8 gives the collumns and / 8 gives rows.  
    int r_off[] = {-2, -2, -1, -1, 1, 1, 2, 2}; // Y coord 
    int c_off[] = {-1, 1, -2, 2, -2, 2, -1, 1}; // Representation of rules for piece, offset arrays. X
    for (int i = 0; i < 8; i++) {
        int tr = rank + r_off[i], tf = file + c_off[i];
        if (tr >= 0 && tr < 8 && tf >= 0 && tf < 8) set_bit(attacks, tr * 8 + tf); // IMPORTANT, PREVENTS CLIPPING -> if all good then 2d to 1d index and flip bit to 1. 
        // the function basically returns all possible landing spaces AS A BITBOARD CALLED ATTACKS. 
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

// ----------------------------------------------------------------
// SLIDING PIECE ATTACKS (Physics)
// ----------------------------------------------------------------
// Bishop Ray-Casting: Diagonal movement -> basically a really fancy and cool way of checking square-by-square the direction a bishop is going. 
uint64_t mask_bishop_attacks(int square, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int tr = square / 8, tf = square % 8;
    int r, f;
    // Top-Right, Top-Left, Bottom-Right, Bottom-Left 
    // WHAT WE ARE DOING first we set the square as attacked we then check the occupancy if there is something we break the loop there. 
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

// Queen is just Rook + Bishop combined OR bitwise. 
uint64_t mask_queen_attacks(int square, uint64_t occupancy) {
    return mask_rook_attacks(square, occupancy) | mask_bishop_attacks(square, occupancy);
}

int is_square_attacked(Board *board, int square, int attacker_side) {
    if (attacker_side == WHITE) {
        if (pawn_attacks[BLACK][square] & board->bitboards[P]) return 1;
    } else {
        if (pawn_attacks[WHITE][square] & board->bitboards[P]) return 1; 
    }

    if (knight_attacks[square] & ((attacker_side == WHITE) ? board->bitboards[N] : board->bitboards[n])) return 1;
    if (king_attacks[square] & ((attacker_side == WHITE) ? board->bitboards[K] : board->bitboards[k])) return 1;

    uint64_t occupancy = board->occupancies[BOTH];

    uint64_t b_attacks = mask_bishop_attacks(square, occupancy);
    uint64_t b_targets = (attacker_side == WHITE) ? (board->bitboards[B] | board->bitboards[Q]) : (board->bitboards[b] | board->bitboards[q]);
    if (b_attacks & b_targets) return 1;

    uint64_t r_attacks = mask_rook_attacks(square, occupancy);
    uint64_t r_targets = (attacker_side == WHITE) ? (board->bitboards[R] | board->bitboards[Q]) : (board->bitboards[r] | board->bitboards[q]);
    if (r_attacks & r_targets) return 1;

    return 0;
}

// ----------------------------------------------------------------
// BOARD HELPERS & FEN PARSER
// ----------------------------------------------------------------
// Uses OR to put all 12 pieces bitboards into one single 64 bit map. Basically a kind of physical floor where all pieces are. 
uint64_t get_occupancy(Board *board) { // NOT SUPER EFFICIENT RN WILL UPDATE 
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < 12; i++) occupancy |= board->bitboards[i];
    return occupancy;
}

int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1;
}

// FEN HELPERS & PARSER  -> FEN (Forsyth-Edwards Notation) according to Google (;-;) looks something like this rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w. 
// in the long output, the letters represent pieces, the numbers represent empty squres, w or b is just whose turn. 
// 8 [r][n][b][q][k][b][n][r] (黒)
// 7 [p][p][p][p][p][p][p][p]
// 6 [ ][ ][ ][ ][ ][ ][ ][ ]
// 5 [ ][ ][ ][ ][ ][ ][ ][ ]
// 4 [ ][ ][ ][ ][P][ ][ ][ ]  <-- 白のポーンが e4 に移動
// 3 [ ][ ][ ][ ][ ][ ][ ][ ]
// 2 [P][P][P][P][ ][P][P][P]
// 1 [R][N][B][Q][K][B][N][R] (白)
//     a  b  c  d  e  f  g  h
// representation of rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w as a chess board. 

// ----------------------------------------------------------------
// BOARD HELPERS & FEN PARSER
// ----------------------------------------------------------------
void parse_fen(char *fen, Board *board) { 
    memset(board, 0, sizeof(Board)); 
    board->enpassant = -1; 
    int square = 0; 
    while (*fen != ' ') { 
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
            int piece;
            char c = *fen;
            if (c == 'P') piece = P; else if (c == 'N') piece = N; else if (c == 'B') piece = B;
            else if (c == 'R') piece = R; else if (c == 'Q') piece = Q; else if (c == 'K') piece = K;
            else if (c == 'p') piece = p; else if (c == 'n') piece = n; else if (c == 'b') piece = b;
            else if (c == 'r') piece = r; else if (c == 'q') piece = q; else if (c == 'k') piece = k;
            set_bit(board->bitboards[piece], square);
            square++;
        } else if (*fen >= '1' && *fen <= '8') {
            square += (*fen - '0'); 
        }
        fen++; 
    }
    fen++;
    board->side = (*fen == 'w') ? WHITE : BLACK; 
    for (int p_idx = P; p_idx <= K; p_idx++) board->occupancies[WHITE] |= board->bitboards[p_idx];
    for (int p_idx = p; p_idx <= k; p_idx++) board->occupancies[BLACK] |= board->bitboards[p_idx];
    board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// ----------------------------------------------------------------
// GAME LOGIC (Move Gen & Init)
// ----------------------------------------------------------------
// == WEEK 5 ==
// THE MOVE GENERATOR
// This function combines physics (Week 4) with rules (Week 5)
void generate_moves(Board *board, MoveList *move_list) {
    move_list->count = 0;
    int source_square, target_square;
    uint64_t bitboard, attacks;

    // 1. PAWNS
    int p_piece = (board->side == WHITE) ? P : p;
    bitboard = board->bitboards[p_piece];
    while (bitboard) {
        source_square = __builtin_ctzll(bitboard);
        // Single Push
        target_square = (board->side == WHITE) ? source_square - 8 : source_square + 8;
        if (target_square >= 0 && target_square <= 63 && !get_bit(board->occupancies[BOTH], target_square)) {
            add_move(move_list, encode_move(source_square, target_square, p_piece, 0, 0, 0, 0, 0));
            // Double Push
            int start_rank = (board->side == WHITE) ? (source_square >= 48 && source_square <= 55) : (source_square >= 8 && source_square <= 15);
            int double_target = (board->side == WHITE) ? target_square - 8 : target_square + 8;
            if (start_rank && !get_bit(board->occupancies[BOTH], double_target))
                add_move(move_list, encode_move(source_square, double_target, p_piece, 0, 0, 1, 0, 0));
        }
        // Captures
        attacks = pawn_attacks[board->side][source_square] & board->occupancies[(board->side == WHITE) ? BLACK : WHITE];
        while (attacks) {
            target_square = __builtin_ctzll(attacks);
            add_move(move_list, encode_move(source_square, target_square, p_piece, 0, 1, 0, 0, 0));
            attacks &= attacks - 1;
        }
        bitboard &= bitboard - 1;
    }

    // 2. KNIGHTS, KINGS, & SLIDERS
    int pieces[] = { (board->side == WHITE) ? N : n, (board->side == WHITE) ? K : k, 
                     (board->side == WHITE) ? B : b, (board->side == WHITE) ? R : r, (board->side == WHITE) ? Q : q };
    
    for (int i = 0; i < 5; i++) {
        int piece = pieces[i];
        bitboard = board->bitboards[piece];
        while (bitboard) {
            source_square = __builtin_ctzll(bitboard);
            if (piece == N || piece == n) attacks = knight_attacks[source_square];
            else if (piece == K || piece == k) attacks = king_attacks[source_square];
            else if (piece == B || piece == b) attacks = mask_bishop_attacks(source_square, board->occupancies[BOTH]);
            else if (piece == R || piece == r) attacks = mask_rook_attacks(source_square, board->occupancies[BOTH]);
            else attacks = mask_queen_attacks(source_square, board->occupancies[BOTH]);

            attacks &= ~board->occupancies[board->side]; // No friendly fire
            while (attacks) {
                target_square = __builtin_ctzll(attacks);
                int capture = get_bit(board->occupancies[(board->side == WHITE) ? BLACK : WHITE], target_square);
                add_move(move_list, encode_move(source_square, target_square, piece, 0, capture, 0, 0, 0));
                attacks &= attacks - 1;
            }
            bitboard &= bitboard - 1;
        }
    }
}
// ============

// PRE COMPUTATION FOR BETTER EFFICIENCY DURING THE GAME
// THIS BASICALLY CREATES A MAP? TO GIVE ALL POSSIBLE MOVES -> I WILL ADD THE SLIDERS IN WEEK 5 or 6
void init_all() { // Initialization 
    for (int i = 0; i < 64; i++) { // Iterates through every single square on the board 
        // == WEEK 5 ==
        pawn_attacks[WHITE][i] = mask_pawn_attacks(WHITE, i);
        pawn_attacks[BLACK][i] = mask_pawn_attacks(BLACK, i);
        // ============
        knight_attacks[i] = mask_knight_attacks(i);
        king_attacks[i] = mask_king_attacks(i);
    }
}

// ----------------------------------------------------------------
// DEBUG TOOLS
// ----------------------------------------------------------------
// Basically the function that makes everything readable to the human into a big big string into an array. 
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
// My bitboard is a flat line of 64 bits. To show it as a board I have to mathematically wrap that line every 8 bits. This formula maps the 2D visual grid back to the 1D physical memory.

// ----------------------------------------------------------------
// MAIN 
// ----------------------------------------------------------------
int main() {
    init_all();
    Board board;
    parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
    
    MoveList moves;
    generate_moves(&board, &moves);
    printf("--- WEEK 6: FULL GENERATOR TEST ---\n");
    printf("Total moves for starting position (Pseudo-Legal): %d\n", moves.count); // Should be 20

    return 0;
}



// if (capture) {
    //     int start = (board->side == WHITE) ? p : P;
    //     int end = (board->side == WHITE) ? k : K;
    //     for (int i = start; i <= end; i++) {
    //         if (get_bit(board->bitboards[i], target)) {
    //             clear_bit(board->bitboards[i], target);
    //             break;
    //         }
    //     }
    // }



    // int source = get_move_source(move);
    // int target = get_move_target(move);
    // int piece = get_move_piece(move);
    // int capture = get_move_capture(move);

    
// DEBUGGED WITH HELP OF GEMINI 3.1 PRO FOR WEEK 5