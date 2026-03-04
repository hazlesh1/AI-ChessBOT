#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Date: 2026/02/21
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 4 – Occupancy & Sliding Piece Attacks (Rook, Bishop, Queen)


// Some notes: AND -> both, OR -> either, XOR -> different, NOT -> flip, PARALLEL bitboard, 12 layers for more efficiency 

// ================================
// MACROS (inline bit helpers) 
// ================================
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)  // 1ULL << square moves bits to left (e.g. 3 -> 00001000 or 2)
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

// ================================
// ENUMS (Pieces as numbers)
// ================================
enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };

// ================================
// BOARD STRUCT (Game State)
// ================================
typedef struct { // New data type name structure 
    uint64_t bitboards[12]; // Array of 12 unsigned 64 bit integers (6x2 piece types), each uint is 8 bytes meaning equals to 96 bytes 
    int side;  // turn indicator 
} Board;
// This whole thing can be visualized by LAYERS, so basically each types of pieces have their own bitboards, making it efficient. 

// ================================
// FEN HELPERS & PARSER  -> FEN (Forsyth-Edwards Notation) according to Google looks something like this rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w. 
// in the long output, the letters represent pieces, the numbers represent empty squres, w or b is just whose turn. 
// ================================

// ================================
// 8 [r][n][b][q][k][b][n][r] (黒)
// 7 [p][p][p][p][p][p][p][p]
// 6 [ ][ ][ ][ ][ ][ ][ ][ ]
// 5 [ ][ ][ ][ ][ ][ ][ ][ ]
// 4 [ ][ ][ ][ ][P][ ][ ][ ]  <-- 白のポーンが e4 に移動
// 3 [ ][ ][ ][ ][ ][ ][ ][ ]
// 2 [P][P][P][P][ ][P][P][P]
// 1 [R][N][B][Q][K][B][N][R] (白)
//    a  b  c  d  e  f  g  h
// representation of rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w as a chess board. 
// ================================

// translator basically enum 
int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1;
}

// THIS DOESNT HAVE THE RULES IMPLEMENTED YET. ONLY PHYSICS. yeah theres castling and en passant and draws 
void parse_fen(char *fen, Board *board) { // address of FEN and board :D 
    memset(board, 0, sizeof(Board)); // An entire board with all zeros
    int square = 0; 
    while (*fen != ' ') { // Focus on Piece Placement 
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
            int piece = char_to_piece(*fen); 
            set_bit(board->bitboards[piece], square);
            square++;
            // If the character is a letter, it uses my char_to_piece function to find the ID, calls the set bit macro to flip a 1 onto the correct bitboard layer 
            // at the current square index, and then it moves to the next square. 
        } else if (*fen >= '1' && *fen <= '8') {
            square += (*fen - '0'); // ASCII to int 0 is 48 and yeah basically a converter 
            // if it is a digit x, it means there are x empty squares. 
            // then skips the square counter forward by that amount 
        }
        fen++; // Next byte in the RAM 
    }
    fen++;
    board->side = (*fen == 'w') ? WHITE : BLACK; // Who 
}

// ================================
// LEAPER ATTACK TABLES (Week 3)
// ================================

// Array of 64 bitboards -> calculating every moves a piece can take, looks at a position then calculates. Basically a lookup table.
uint64_t knight_attacks[64];
uint64_t king_attacks[64];

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

// exact same here but for king 
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

// Uses OR to put all 12 pieces bitboards into one single 64 bit map. Basically a kind of physical floor where all pieces are. 
uint64_t get_occupancy(Board *board) { // NOT SUPER EFFICIENT RN WILL UPDATE 
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < 12; i++) occupancy |= board->bitboards[i];
    return occupancy;
}

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

// ================================
// INITIALIZATION & DEBUG 
// ================================

// PRE COMPUTATION FOR BETTER EFFICIENCY DURING THE GAME
// THIS BASICALLY CREATES A MAP? TO GIVE ALL POSSIBLE MOVES -> I WILL ADD THE SLIDERS IN WEEK 5 or 6
void init_all() { // Initialization 
    for (int i = 0; i < 64; i++) { // Iterates through every single square on the board 
        knight_attacks[i] = mask_knight_attacks(i);
        king_attacks[i] = mask_king_attacks(i);
    }
}

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
    printf("\n     a  b  c  d  e  f  g  h\n\n");
}
// My bitboard is a flat line of 64 bits. To show it as a board I have to mathematically wrap that line every 8 bits. This formula maps the 2D visual grid back to the 1D physical memory.

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