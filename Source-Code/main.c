#include <stdio.h>
#include <stdint.h>
#include <string.h> // Added for memset

// Date: 2026/02/01
// Define a macro to get bit values at specific square
#define get_bit(bitboard, square) ((bitboard & (1ULL << square))? 1 : 0)
// Added Week 2: Macro to set a bit
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))

// --- Week 2 Additions: Board Definitions ---
enum { P, N, B, R, Q, K, p, n, b, r, q, k }; // White (upper), Black (lower)
enum { WHITE, BLACK, BOTH };

typedef struct {
    uint64_t bitboards[12];  
    int side;                
} Board;

// Map characters to our enum pieces
int char_to_piece(char c) {
    if (c == 'P') return P; if (c == 'N') return N; if (c == 'B') return B;
    if (c == 'R') return R; if (c == 'Q') return Q; if (c == 'K') return K;
    if (c == 'p') return p; if (c == 'n') return n; if (c == 'b') return b;
    if (c == 'r') return r; if (c == 'q') return q; if (c == 'k') return k;
    return -1;
}

// --- Week 2 Additions: The FEN Parser ---
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

// Main function to print the bitboard as a chess board
// Updated: Now accepts a Board struct to visualize the whole game
void print_full_board(Board *board) {
    printf("\n");
    for (int rows = 0; rows < 8; rows++){
        for (int columns = 0; columns < 8; columns++) {
            int square = rows * 8 + columns;
            if (!columns) printf(" %d ", 8 - rows);

            char piece_char = '.';
            char pieces_glyph[] = "PNBRQKpnbrqk";
            for (int i = 0; i < 12; i++) {
                if (get_bit(board->bitboards[i], square)) {
                    piece_char = pieces_glyph[i];
                    break;
                }
            }
            printf(" %c ", piece_char);
        }
        printf("\n");
    }
    printf("\n     a  b  c  d  e  f  g  h\n\n");
    printf("     Side to move: %s\n", board->side == WHITE ? "white" : "black");
}

/* Original print_bitboard kept for documentation 
void print_bitboard(uint64_t bitboard) {
    printf("\n");
    for (int rows = 0; rows < 8; rows++){
        for (int columns = 0; columns < 8; columns++) {
            int square = rows * 8 + columns;
            if (!columns) printf(" %d ", 8 - rows);
            printf(" %d ", get_bit(bitboard, square));
        }
        printf("\n");
    }
    printf("\n     a  b  c  d  e  f  g  h\n\n");
    printf("     Bitboard Value: %llu\n\n", bitboard);
}
*/

int main(){
    /* --- Week 1 Logic (Commented out for documentation) ---
    // Defs a bitboard with bits set at index 0, 1 and 8
    uint64_t my_pawns = 0;

    // Placing pawns using bitwise
    my_pawns |= (1ULL << 0); 
    my_pawns |= (1ULL << 8); 
    my_pawns |= (1ULL << 63);

    print_bitboard(my_pawns);
    ------------------------------------------------------- */

    // --- Week 2 Implementation ---
    Board board;
    char *start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    
    parse_fen(start_pos, &board);
    print_full_board(&board);

    return 0;
}