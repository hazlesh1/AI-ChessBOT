#include <stdio.h>
#include <stdint.h>

// Define a macro to get bit values at specific square
#define get_bit(bitboard, square) ((bitboard & (1ULL << square))? 1 : 0)

// Main function to print the bitboard as a chess board
void print_bitboard(uint64_t bitboard) {
    printf("\n");

    for (int rows = 0; rows < 8; rows++){
        for (int columns = 0; columns < 8; columns++) {

            // Mapping 2D coordinates to 1D index (0-63)
            int square = rows * 8 + columns;

            // Printing row num on the left
            if (!columns) printf(" %d ", 8 - rows);

            // If bit is 0 or 1
            printf(" %d ", get_bit(bitboard, square));
        }
        printf("\n");
    }
    printf("\n     a  b  c  d  e  f  g  h\n\n");
    printf("     Bitboard Value: %llu\n\n", bitboard);
}

int main(){
    // Defs a bitboard with bits set at index 0, 1 and 8
    uint64_t my_pawns = 0;

    // Placing pawns using bitwise
    my_pawns |= (1ULL << 0); 
    my_pawns |= (1ULL << 8); 
    my_pawns |= (1ULL << 63);

    print_bitboard(my_pawns);

    return 0;
}