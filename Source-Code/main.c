#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Date: 2026/02/21
// Dev: Leo Girard
// Project: Bitboard Chess Engine
// Week 6 – Completing the Move Generator & Legality 


// ----------------------------------------------------------------
// MACROS & CONSTANTS
// ----------------------------------------------------------------
#define get_bit(bitboard, square) ((bitboard & (1ULL << square)) ? 1 : 0)
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define clear_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling, captured_piece) \
    ((source) | ((target) << 6) | ((piece) << 12) | ((promoted) << 16) | \
    ((captured_piece) << 24) | \
    ((capture) << 20) | ((double_push) << 21) | ((enpassant) << 22) | ((castling) << 23))

#define get_move_source(move) ((move) & 0x3f)
#define get_move_target(move) (((move) >> 6) & 0x3f)
#define get_move_piece(move) (((move) >> 12) & 0xf)
#define get_move_capture(move) (((move) >> 20) & 0x1)
#define get_move_castling(move) (((move) >> 23) & 0x1)
#define get_move_captured_piece(move) (((move) >> 24) & 0xf)


enum { P, N, B, R, Q, K, p, n, b, r, q, k };
enum { WHITE, BLACK, BOTH };

enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1
};

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

const int pawn_pst[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int castling_rights_update[64] = {
    7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};

// ----------------------------------------------------------------
// DATA STRUCTURES
// ----------------------------------------------------------------
typedef struct {
    uint32_t moves[256];
    int count;
} MoveList;

typedef struct {
    int castling;
    int enpassant;
} GameState;

typedef struct { 
    uint64_t bitboards[12]; 
    int side;  
    uint64_t occupancies[3]; 
    int enpassant;           
    int castling; 
    GameState history[1024];
    int history_ptr;           
} Board;

static inline void add_move(MoveList *move_list, uint32_t move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}


// ----------------------------------------------------------------
// GLOBAL ATTACK TABLES
// ----------------------------------------------------------------
uint64_t knight_attacks[64];
uint64_t king_attacks[64];
uint64_t pawn_attacks[2][64];

// ----------------------------------------------------------------
// LEAPER ATTACK MASKING (Physics)
// ----------------------------------------------------------------
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

// ----------------------------------------------------------------
// SLIDING PIECE ATTACKS (Physics)
// ----------------------------------------------------------------
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

// ----------------------------------------------------------------
// == WEEK 6 == ATTACK DETECTION (Ghost Pieces)
// ----------------------------------------------------------------
int is_square_attacked(Board *board, int square, int attacker_side) {
    if (attacker_side == WHITE) {
        if (pawn_attacks[BLACK][square] & board->bitboards[P]) return 1;
    } else {
        if (pawn_attacks[WHITE][square] & board->bitboards[p]) return 1; 
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

void unmake_move(Board *board, uint32_t move);
void make_move(Board *board, uint32_t move);

// ----------------------------------------------------------------
// == WEEK 6 == MAKE MOVE (The Physics of Action)
// ----------------------------------------------------------------
void make_move(Board *board, uint32_t move) {
    // 1. SAVE HISTORY
    board->history[board->history_ptr].castling = board->castling;
    board->history[board->history_ptr].enpassant = board->enpassant;
    board->history_ptr++;


    int src = get_move_source(move);
    int trg = get_move_target(move);
    int pce = get_move_piece(move);
    int promo_pce = ((move >> 16) & 0xf);
    int cap = get_move_capture(move);
    int cap_pce = get_move_captured_piece(move);
    int double_push = (move >> 21) & 0x1;
    int enpassant = (move >> 22) & 0x1;
    int castling = get_move_castling(move);


    clear_bit(board->bitboards[pce], src);
    if (promo_pce) {
        set_bit(board->bitboards[promo_pce], trg);
    } else {
        set_bit(board->bitboards[pce], trg);
    }

 
    if (enpassant) {
        int ep_sq = (board->side == WHITE) ? trg + 8 : trg - 8;
        clear_bit(board->bitboards[cap_pce], ep_sq);
    } else if (cap) {
        clear_bit(board->bitboards[cap_pce], trg);
    }


    if (castling) {
        switch (trg) {
            case g1: clear_bit(board->bitboards[R], h1); set_bit(board->bitboards[R], f1); break;
            case c1: clear_bit(board->bitboards[R], a1); set_bit(board->bitboards[R], d1); break;
            case g8: clear_bit(board->bitboards[r], h8); set_bit(board->bitboards[r], f8); break;
            case c8: clear_bit(board->bitboards[r], a8); set_bit(board->bitboards[r], d8); break;
        }
    }


    board->enpassant = -1;
    if (double_push) {
        board->enpassant = (board->side == WHITE) ? trg + 8 : trg - 8;
    }

    board->castling &= castling_rights_update[src];
    board->castling &= castling_rights_update[trg];

    memset(board->occupancies, 0, sizeof(board->occupancies));
    for (int i = P; i <= K; i++) board->occupancies[WHITE] |= board->bitboards[i];
    for (int i = p; i <= k; i++) board->occupancies[BLACK] |= board->bitboards[i];
    board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
    
    board->side ^= 1;
}
// ----------------------------------------------------------------
// BOARD HELPERS & FEN PARSER
// ----------------------------------------------------------------
void parse_fen(char *fen, Board *board) { 
    memset(board, 0, sizeof(Board)); 
    board->enpassant = -1; 
    int square = 0; 
    
    // parse Pieces
    while (*fen != ' ') { 
        if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
            int piece; char c = *fen;
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
    
    // parse Side to Move
    board->side = (*fen == 'w') ? WHITE : BLACK; 
    fen += 2; 
    
    // parse Castling 
    board->castling = 0;
    while (*fen != ' ' && *fen != '\0') {
        switch (*fen) {
            case 'K': board->castling |= 1; break;
            case 'Q': board->castling |= 2; break;
            case 'k': board->castling |= 4; break;
            case 'q': board->castling |= 8; break;
            case '-': break;
        }
        fen++;
    }

    // Init Occupancies
    for (int i = P; i <= K; i++) board->occupancies[WHITE] |= board->bitboards[i];
    for (int i = p; i <= k; i++) board->occupancies[BLACK] |= board->bitboards[i];
    board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// ----------------------------------------------------------------
// GAME LOGIC (Move Gen & Init)
// ----------------------------------------------------------------
void generate_moves(Board *board, MoveList *move_list) {
    move_list->count = 0;

    int source_square, target_square;
    uint64_t bitboard, attacks;

    int pieces[] = {
        (board->side == WHITE) ? P : p,
        (board->side == WHITE) ? N : n,
        (board->side == WHITE) ? B : b,
        (board->side == WHITE) ? R : r,
        (board->side == WHITE) ? Q : q,
        (board->side == WHITE) ? K : k
    };

    for (int i = 0; i < 6; i++) {
        int piece = pieces[i];
        bitboard = board->bitboards[piece];

        while (bitboard) {

            source_square = __builtin_ctzll(bitboard);

                if (piece == P || piece == p) {

                    target_square = (board->side == WHITE) ? source_square - 8 : source_square + 8;

                    if (target_square >= 0 && target_square <= 63 && !get_bit(board->occupancies[BOTH], target_square)) {

                        if ((board->side == WHITE && target_square <= h8) || (board->side == BLACK && target_square >= a1)) {
                            int promos[] = { (board->side == WHITE ? Q : q), (board->side == WHITE ? R : r), (board->side == WHITE ? B : b), (board->side == WHITE ? N : n) };
                            for (int i = 0; i < 4; i++) add_move(move_list, encode_move(source_square, target_square, piece, promos[i], 0, 0, 0, 0, 0));
                        } else {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 0, 0, 0, 0, 0));
                            // double push
                            int start_rank = (board->side == WHITE) ? (source_square >= a2 && source_square <= h2) : (source_square >= a7 && source_square <= h7);
                            int d_target = (board->side == WHITE) ? target_square - 8 : target_square + 8;
                            if (start_rank && !get_bit(board->occupancies[BOTH], d_target))
                                add_move(move_list, encode_move(source_square, d_target, piece, 0, 0, 1, 0, 0, 0));
                        }
                    }

                    attacks = pawn_attacks[board->side][source_square];
                    while (attacks) {
                        target_square = __builtin_ctzll(attacks);
                        int capture = get_bit(board->occupancies[board->side ^ 1], target_square);
                        
                        if (target_square == board->enpassant) {
                            add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 1, 0, (board->side == WHITE ? p : P)));
                        } else if (capture) {
                            int captured_piece = -1;
                            for (int p_idx = (board->side == WHITE ? p : P); p_idx <= (board->side == WHITE ? k : K); p_idx++) {
                                if (get_bit(board->bitboards[p_idx], target_square)) { captured_piece = p_idx; break; }
                            }
                            if ((board->side == WHITE && target_square <= h8) || (board->side == BLACK && target_square >= a1)) {
                                int promos[] = { (board->side == WHITE ? Q : q), (board->side == WHITE ? R : r), (board->side == WHITE ? B : b), (board->side == WHITE ? N : n) };
                                for (int i = 0; i < 4; i++) add_move(move_list, encode_move(source_square, target_square, piece, promos[i], 1, 0, 0, 0, captured_piece));
                            } else {
                                add_move(move_list, encode_move(source_square, target_square, piece, 0, 1, 0, 0, 0, captured_piece));
                            }
                        }
                        attacks &= attacks - 1;
                    }
                }
        

            // -------------------------
            // NON-PAWN PIECES
            // -------------------------
            else if (piece == N || piece == n)
                attacks = knight_attacks[source_square];

            else if (piece == K || piece == k) {
                attacks = king_attacks[source_square];

                // == CASTLING GEN ==
                if (piece == K) {
                    // White Kingside (bit 1)
                    if (board->castling & 1) {
                        if (!get_bit(board->occupancies[BOTH], f1) && !get_bit(board->occupancies[BOTH], g1)) {
                            if (!is_square_attacked(board, e1, BLACK) && !is_square_attacked(board, f1, BLACK)) {
                                add_move(move_list, encode_move(e1, g1, K, 0, 0, 0, 0, 1, 0));
                            }
                        }
                    }
                    // White Queenside (b. 2)
                    if (board->castling & 2) {
                        if (!get_bit(board->occupancies[BOTH], d1) && !get_bit(board->occupancies[BOTH], c1) && !get_bit(board->occupancies[BOTH], b1)) {
                            if (!is_square_attacked(board, e1, BLACK) && !is_square_attacked(board, d1, BLACK)) {
                                add_move(move_list, encode_move(e1, c1, K, 0, 0, 0, 0, 1, 0));
                            }
                        }
                    }
                } else {
                    // Black Kingside (b. 4)
                    if (board->castling & 4) {
                        if (!get_bit(board->occupancies[BOTH], f8) && !get_bit(board->occupancies[BOTH], g8)) {
                            if (!is_square_attacked(board, e8, WHITE) && !is_square_attacked(board, f8, WHITE)) {
                                add_move(move_list, encode_move(e8, g8, k, 0, 0, 0, 0, 1, 0));
                            }
                        }
                    }
                    // Black Queenside (b. 8)
                    if (board->castling & 8) {
                        if (!get_bit(board->occupancies[BOTH], d8) && !get_bit(board->occupancies[BOTH], c8) && !get_bit(board->occupancies[BOTH], b8)) {
                            if (!is_square_attacked(board, e8, WHITE) && !is_square_attacked(board, d8, WHITE)) {
                                add_move(move_list, encode_move(e8, c8, k, 0, 0, 0, 0, 1, 0));
                            }
                        }
                    }
                }
            }

            else if (piece == B || piece == b)
                attacks = mask_bishop_attacks(source_square, board->occupancies[BOTH]);

            else if (piece == R || piece == r)
                attacks = mask_rook_attacks(source_square, board->occupancies[BOTH]);

            else
                attacks = mask_queen_attacks(source_square, board->occupancies[BOTH]);

            if (piece != P && piece != p)
                attacks &= ~board->occupancies[board->side];

            // -------------------------
            // NORMAL MOVES + CAPTURES
            // -------------------------
            while (attacks) {

                target_square = __builtin_ctzll(attacks);

                int captured_piece = -1;

                int enemy_bb_start = (board->side == WHITE) ? p : P;
                int enemy_bb_end   = (board->side == WHITE) ? k : K;

                int capture = get_bit(
                    board->occupancies[(board->side == WHITE) ? BLACK : WHITE],
                    target_square
                );

                if (capture) {
                    for (int p_idx = enemy_bb_start; p_idx <= enemy_bb_end; p_idx++) {
                        if (get_bit(board->bitboards[p_idx], target_square)) {
                            captured_piece = p_idx;
                            break;
                        }
                    }
                }

                add_move(move_list,
                    encode_move(source_square, target_square, piece,
                                0, capture, 0, 0, 0, captured_piece)
                );

                attacks &= attacks - 1;
            }

            bitboard &= bitboard - 1;
        }
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

const int piece_values[] = { 100, 300, 300, 500, 900, 10000, 100, 300, 300, 500, 900, 10000 };

int evaluate(Board *board) {
    int score = 0;
    uint64_t bitboard;
    int square;

    // WHITE PIECES
    for (int piece = P; piece <= K; piece++) {
        bitboard = board->bitboards[piece];
        while (bitboard) {
            square = __builtin_ctzll(bitboard);
            score += piece_values[piece];     

            if (piece == N) score += knight_pst[square];
            if (piece == P) score += pawn_pst[square];
            
            bitboard &= bitboard - 1; 
        }
    }

    // BLACK PIECES
    for (int piece = p; piece <= k; piece++) {
        bitboard = board->bitboards[piece];
        while (bitboard) {
            square = __builtin_ctzll(bitboard);
            score -= piece_values[piece];      

            int flipped_square = square ^ 56; 
            
            if (piece == n) score -= knight_pst[flipped_square];
            if (piece == p) score -= pawn_pst[flipped_square];
            
            bitboard &= bitboard - 1;
        }
    }

    return (board->side == WHITE) ? score : -score;
}

int alpha_beta(Board *board, int alpha, int beta, int depth) {

    if (depth == 0)
        return evaluate(board);

    MoveList moves;
    generate_moves(board, &moves);

    int legal_moves = 0;

    for (int i = 0; i < moves.count; i++) {

        if (!get_move_capture(moves.moves[i]))
            continue;

        make_move(board, moves.moves[i]);

        int king_piece = (board->side == WHITE) ? k : K;
        int king_square = __builtin_ctzll(board->bitboards[king_piece]);

        if (is_square_attacked(board, king_square, board->side)) {
            unmake_move(board, moves.moves[i]);
            continue;
        }

        legal_moves++;

        int score = -alpha_beta(board, -beta, -alpha, depth - 1);

        unmake_move(board, moves.moves[i]);

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    for (int i = 0; i < moves.count; i++) {

        if (get_move_capture(moves.moves[i]))
            continue;

        make_move(board, moves.moves[i]);

        int king_piece = (board->side == WHITE) ? k : K;
        int king_square = __builtin_ctzll(board->bitboards[king_piece]);

        if (is_square_attacked(board, king_square, board->side)) {
            unmake_move(board, moves.moves[i]);
            continue;
        }

        legal_moves++;

        int score = -alpha_beta(board, -beta, -alpha, depth - 1);

        unmake_move(board, moves.moves[i]);

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    if (legal_moves == 0) {
        int king_piece = (board->side == WHITE) ? K : k;
        int king_square = __builtin_ctzll(board->bitboards[king_piece]);

        if (is_square_attacked(board, king_square, board->side ^ 1)) {
            // - depth A quicker mate (higher remaining depth) 
            return -49000 - depth; 
        } else {
            return 0; 
        }
    }

    return alpha;
}

// uint32_t search_position(Board *board, int depth) {
//     MoveList moves; 
//     generate_moves(board, &moves);

//     uint32_t best_move = 0;
//     int best_score = -50000;

//     for (int i = 0; i < moves.count; i++) {
//         make_move(board, moves.moves[i]);

//         int king_piece = (board->side == WHITE) ? k : K;
//         int king_square = __builtin_ctzll(board->bitboards[king_piece]);

//         if (is_square_attacked(board, king_square, board->side)) {
//             unmake_move(board, moves.moves[i]);
//             continue;
//         }

//         int score = -alpha_beta(board, -50000, 50000, depth - 1);

//         unmake_move(board, moves.moves[i]);

//         if (score > best_score) {
//             best_score = score;
//             best_move = moves.moves[i];
//         }
//     }
//     return best_move; 
// }

uint32_t search_position(Board *board, int depth) {
    MoveList moves; 
    generate_moves(board, &moves);

    uint32_t best_move = 0;
    int best_score = -50000;
    
    // FIX: Initiealiz alpha and beta at the root
    int alpha = -50000;
    int beta = 50000;

    for (int i = 0; i < moves.count; i++) {
        make_move(board, moves.moves[i]);

        int king_piece = (board->side == WHITE) ? k : K;
        int king_square = __builtin_ctzll(board->bitboards[king_piece]);

        if (is_square_attacked(board, king_square, board->side)) {
            unmake_move(board, moves.moves[i]);
            continue;
        }

        // FIX:  -beta and -alpha down 
        int score = -alpha_beta(board, -beta, -alpha, depth - 1);

        unmake_move(board, moves.moves[i]);

        if (score > best_score) {
            best_score = score;
            best_move = moves.moves[i];
        }
        
        // FIX: Update alpha at the root 
        if (score > alpha) {
            alpha = score;
        }
    }
    return best_move; 
}

char piece_chars[] = "PNBRQKpnbrqk";
void print_board(Board *board) {
    printf("\n");
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (file == 0) printf(" %d  ", 8 - rank);
            int piece = -1;
            for (int bb_piece = P; bb_piece <= k; bb_piece++) {
                if (get_bit(board->bitboards[bb_piece], square)) {
                    piece = bb_piece;
                    break;
                }
            }
            if (piece == -1) printf(". ");
            else printf("%c ", piece_chars[piece]);
        }
        printf("\n");
    }
    printf("\n    a b c d e f g h\n\n");
}

int parse_move(Board *board, char *move_string) {
    int source = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;
    int target = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;
    
    int piece = -1;

    for (int bb_piece = P; bb_piece <= k; bb_piece++) {
        if (get_bit(board->bitboards[bb_piece], source)) {
            piece = bb_piece;
            break;
        }
    }


    int capture = 0;
    for (int bb_piece = P; bb_piece <= k; bb_piece++) {
        if (get_bit(board->bitboards[bb_piece], target)) {
            capture = 1;
            break;
        }
    }

    return encode_move(source, target, piece, 0, capture, 0, 0, 0, 0);
}

void print_move(uint32_t move) {
    printf("%c%d%c%d\n", 
           (get_move_source(move) % 8) + 'a', 8 - (get_move_source(move) / 8),
           (get_move_target(move) % 8) + 'a', 8 - (get_move_target(move) / 8));
}

uint32_t get_user_move(Board *board) {
    MoveList legal_moves;
    generate_moves(board, &legal_moves);
    
    MoveList strictly_legal;
    strictly_legal.count = 0;
    for (int i = 0; i < legal_moves.count; i++) {
        Board temp = *board;
        make_move(&temp, legal_moves.moves[i]);
        int king_sq = __builtin_ctzll(temp.bitboards[(board->side == WHITE) ? K : k]);
        if (!is_square_attacked(&temp, king_sq, temp.side)) {
            add_move(&strictly_legal, legal_moves.moves[i]);
        }
    }

    char input[10];
    while (1) {
        printf("Your move (e.g., e2e4 or e7e8q): ");
        if (!fgets(input, sizeof(input), stdin)) return 0;

        if (strlen(input) < 4) continue;

        int start_sq = (input[0] - 'a') + (8 - (input[1] - '0')) * 8;
        int target_sq = (input[2] - 'a') + (8 - (input[3] - '0')) * 8;

        //(default promo queen)
        char promo_char = (strlen(input) >= 5 && input[4] != '\n') ? input[4] : 'q';
        int target_promo = 0;
        
        if (promo_char == 'q') target_promo = (board->side == WHITE) ? Q : q;
        else if (promo_char == 'r') target_promo = (board->side == WHITE) ? R : r;
        else if (promo_char == 'b') target_promo = (board->side == WHITE) ? B : b;
        else if (promo_char == 'n') target_promo = (board->side == WHITE) ? N : n;

        for (int i = 0; i < strictly_legal.count; i++) {
            uint32_t m = strictly_legal.moves[i];
            
            if (get_move_source(m) == start_sq && get_move_target(m) == target_sq) {
                int m_promo = ((m >> 16) & 0xf);
                
                // if promo -> enforce exact match
                if (m_promo) {
                    if (m_promo == target_promo) return m;
                } else {
                    return m; // normal moves
                }
            }
        }
        printf("Invalid move! That is either illegal or your piece isn't there. Try again.\n");
    }
}


void unmake_move(Board *board, uint32_t move) {
    board->side ^= 1;

    int src = get_move_source(move);
    int trg = get_move_target(move);
    int pce = get_move_piece(move);
    int promo_pce = ((move >> 16) & 0xf);
    int cap = get_move_capture(move);
    int cap_pce = get_move_captured_piece(move);
    int enpassant = (move >> 22) & 0x1;
    int castling = get_move_castling(move);


    if (promo_pce) {

        clear_bit(board->bitboards[promo_pce], trg);
    } else {

        clear_bit(board->bitboards[pce], trg);
    }
    set_bit(board->bitboards[pce], src);


    if (enpassant) {
        int ep_sq = (board->side == WHITE) ? trg + 8 : trg - 8;
        set_bit(board->bitboards[cap_pce], ep_sq);
    } else if (cap) {
        set_bit(board->bitboards[cap_pce], trg);
    }


    if (castling) {
        switch (trg) {
            case g1: set_bit(board->bitboards[R], h1); clear_bit(board->bitboards[R], f1); break;
            case c1: set_bit(board->bitboards[R], a1); clear_bit(board->bitboards[R], d1); break;
            case g8: set_bit(board->bitboards[r], h8); clear_bit(board->bitboards[r], f8); break;
            case c8: set_bit(board->bitboards[r], a8); clear_bit(board->bitboards[r], d8); break;
        }
    }


    board->history_ptr--;
    board->castling = board->history[board->history_ptr].castling;
    board->enpassant = board->history[board->history_ptr].enpassant;

    memset(board->occupancies, 0, sizeof(board->occupancies));
    for (int i = P; i <= K; i++) board->occupancies[WHITE] |= board->bitboards[i];
    for (int i = p; i <= k; i++) board->occupancies[BLACK] |= board->bitboards[i];
    board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// ----------------------------------------------------------------
// STATLESS MAIN FOR WEB 
// ----------------------------------------------------------------
int main(int argc, char *argv[]) {
    init_all();
    Board board;

    if (argc > 1) {
        parse_fen(argv[1], &board);
    } else {
        parse_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &board);
    }

    int king_piece = (board.side == WHITE) ? K : k;
    if (board.bitboards[king_piece] == 0) {
        printf("error_no_king\n");
        return 0;
    }

    uint32_t move = search_position(&board, 6); 

    if (move != 0) {
        print_move(move);
    } else {
        printf("none\n");
    }

    fflush(stdout); 
    return 0;
}