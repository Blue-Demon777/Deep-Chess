#include<bits/stdc++.h>
#include <chess.hpp>

using namespace chess;

// Engine Constants
constexpr int INF = 100000000;
constexpr int MATE_SCORE = 20000;

// --- Piece-Square Tables ---
// Converted to constexpr arrays for performance optimization
constexpr int pawnTable[64] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, -10, -10, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    5, 5, 10, 20, 20, 10, 5, 5,
    10, 10, 10, 20, 20, 10, 10, 10,
    20, 20, 20, 30, 30, 30, 20, 20,
    30, 30, 30, 40, 40, 30, 30, 30,
    90, 90, 90, 90, 90, 90, 90, 90
};

constexpr int knightTable[64] = {
    -5, -10, 0, 0, 0, 0, -10, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 5, 20, 10, 10, 20, 5, -5,
    -5, 10, 20, 30, 30, 20, 10, -5,
    -5, 10, 20, 30, 30, 20, 10, -5,
    -5, 5, 20, 20, 20, 20, 5, -5,
    -5, 0, 0, 10, 10, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5
};

constexpr int bishopTable[64] = {
    0, 0, -10, 0, 0, -10, 0, 0,
    0, 30, 0, 0, 0, 0, 30, 0,
    0, 10, 0, 0, 0, 0, 10, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, 10, 10, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

constexpr int rookTable[64] = {
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, 5, 5, 0, 0, 0
};

constexpr int queenTable[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -10, 0, 5, 5, 5, 5, 0, -10,
    -5, 0, 5, 10, 10, 5, 0, -5,
    -5, 0, 5, 10, 10, 5, 0, -5,
    -10, 0, 5, 5, 5, 5, 0, -10,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -20, -10, -10, -5, -5, -10, -10, -20
};
// ------------------------------

class TranspositionTable {
public:
    enum Flag : uint8_t { EXACT, LOWERBOUND, UPPERBOUND };

    struct Entry {
        uint64_t key = 0;
        Move bestMove = Move::NULL_MOVE;
        int score = 0;
        int depth = -1;
        Flag flag = EXACT;
    };

    TranspositionTable(size_t sizePowerOfTwo = 20) {
        table.resize(1 << sizePowerOfTwo);
        mask = (1 << sizePowerOfTwo) - 1;
    }

    bool probe(uint64_t key, Entry& entry) const {
        size_t idx = key & mask;
        if (table[idx].key == key) {
            entry = table[idx];
            return true;
        }
        return false;
    }

    void store(uint64_t key, int depth, int score, Flag flag, Move bestMove) {
        size_t idx = key & mask;
        // Depth-preferred replacement strategy
        if (table[idx].key != key || depth >= table[idx].depth) {
            table[idx] = {key, bestMove, score, depth, flag};
        }
    }

    void clear() {
        std::fill(table.begin(), table.end(), Entry{});
    }

private:
    std::vector<Entry> table;
    size_t mask;
};

class Engine {
public:
    Engine() : tt(20) {}

    void run() {
        board.setFen(constants::STARTPOS);
        std::string line;

        while (std::getline(std::cin, line)) {
            if (line == "uci") {
                std::cout << "id name chessBot1\nid author kanishk\nuciok" << std::endl;
            } 
            else if (line == "isready") {
                std::cout << "readyok" << std::endl;
            } 
            else if (line == "ucinewgame") {
                board.setFen(constants::STARTPOS);
                tt.clear();
            } 
            else if (line.rfind("position", 0) == 0) {
                parsePosition(line);
            } 
            else if (line.rfind("go", 0) == 0) {
                parseGo(line);

                if (board.sideToMove() == Color::WHITE) {
                    timeLimitMs = moveBudget(wtime, winc);
                } else {
                    timeLimitMs = moveBudget(btime, binc);
                }

                Move bestMove = search();

                if (bestMove == Move::NULL_MOVE) {
                    std::cout << "bestmove 0000" << std::endl;
                } else {
                    std::cout << "bestmove " << uci::moveToUci(bestMove) << std::endl;
                }
            } 
            else if (line == "quit") {
                break;
            }
        }
    }

private:
    Board board;
    TranspositionTable tt;

    std::chrono::steady_clock::time_point searchStart;
    int timeLimitMs = 0;
    bool stopSearch = false;

    int wtime = 0, btime = 0, winc = 0, binc = 0;

    int moveBudget(int rem, int inc) {
        int moveTime = rem / 30 + inc * 3 / 4;
        moveTime = std::min(moveTime, rem / 5);
        return std::max(10, moveTime);
    }

    bool outOfTime() {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - searchStart).count();
        return elapsed >= timeLimitMs;
    }

    int pieceValue(PieceType pt) const {
        switch (pt.internal()) {
            case PieceType::PAWN:   return 100;
            case PieceType::KNIGHT: return 300;
            case PieceType::BISHOP: return 300;
            case PieceType::ROOK:   return 500;
            case PieceType::QUEEN:  return 1000;
            default:                return 0;
        }
    }

    int scoreToTT(int score, int ply) const {
        if (score >= MATE_SCORE - 100) return score + ply;
        if (score <= -MATE_SCORE + 100) return score - ply;
        return score;
    }

    int scoreFromTT(int score, int ply) const {
        if (score >= MATE_SCORE - 100) return score - ply;
        if (score <= -MATE_SCORE + 100) return score + ply;
        return score;
    }

    void orderMoves(Movelist &moves) {
        TranspositionTable::Entry ttEntry;
        Move hashMove = Move::NULL_MOVE;

        if (tt.probe(board.hash(), ttEntry)) {
            hashMove = ttEntry.bestMove;
        }

        for (auto &move : moves) {
            int score = 0;

            if (move == hashMove) {
                score += 20000;
            }

            if (board.isCapture(move)) {
                Piece victim = board.at(move.to());
                Piece attacker = board.at(move.from());
                score += 10 * pieceValue(victim.type()) - pieceValue(attacker.type());
            }

            move.setScore(score);
        }

        std::sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
            return a.score() > b.score();
        });
    }

    int evaluate() const {
        int score = 0;
        int whiteBishop = 0;
        int blackBishop = 0;

        for (int sq = 0; sq < 64; sq++) {
            Piece p = board.at(Square(sq));
            if (p == Piece::NONE) continue;

            int val = pieceValue(p.type());

            if (p.type() == PieceType::BISHOP) {
                if (p.color() == Color::WHITE) whiteBishop++;
                else blackBishop++;
            }

            if (p.color() == Color::WHITE) {
                score += val;
                switch (p.type().internal()) {
                    case PieceType::PAWN:   score += pawnTable[sq]; break;
                    case PieceType::KNIGHT: score += knightTable[sq]; break;
                    case PieceType::BISHOP: score += bishopTable[sq]; break;
                    case PieceType::ROOK:   score += rookTable[sq]; break;
                    case PieceType::QUEEN:  score += queenTable[sq]; break;
                }
            } else {
                score -= val;
                switch (p.type().internal()) {
                    case PieceType::PAWN:   score -= pawnTable[sq ^ 56]; break;
                    case PieceType::KNIGHT: score -= knightTable[sq ^ 56]; break;
                    case PieceType::BISHOP: score -= bishopTable[sq ^ 56]; break;
                    case PieceType::ROOK:   score -= rookTable[sq ^ 56]; break;
                    case PieceType::QUEEN:  score -= queenTable[sq ^ 56]; break;
                }
            }
        }

        if (whiteBishop >= 2) score += 30;
        if (blackBishop >= 2) score -= 30;

        return score;
    }

    int minimax(int depth, int ply, int alpha, int beta, bool maximizingPlayer) {
        if (outOfTime()) {
            stopSearch = true;
            return 0;
        }

        int alphaOrig = alpha;
        int betaOrig = beta;
        uint64_t key = board.hash();
        TranspositionTable::Entry ttEntry;

        if (tt.probe(key, ttEntry) && ttEntry.depth >= depth) {
            int ttScore = scoreFromTT(ttEntry.score, ply);
            if (ttEntry.flag == TranspositionTable::EXACT) {
                return ttScore;
            } else if (ttEntry.flag == TranspositionTable::LOWERBOUND) {
                alpha = std::max(alpha, ttScore);
            } else if (ttEntry.flag == TranspositionTable::UPPERBOUND) {
                beta = std::min(beta, ttScore);
            }

            if (alpha >= beta) {
                return ttScore;
            }
        }

        Movelist moves;
        movegen::legalmoves(moves, board);
        orderMoves(moves);

        if (moves.empty()) {
            if (board.inCheck()) {
                return maximizingPlayer ? -MATE_SCORE + ply : MATE_SCORE - ply;
            }
            return 0;
        }

        if (depth == 0) {
            int eval = evaluate();
            tt.store(key, depth, scoreToTT(eval, ply), TranspositionTable::EXACT, Move::NULL_MOVE);
            return eval;
        }

        int best = maximizingPlayer ? -INF : INF;
        Move bestMove = Move::NULL_MOVE;

        for (const Move &move : moves) {
            if (stopSearch) break;

            board.makeMove(move);
            int score = minimax(depth - 1, ply + 1, alpha, beta, !maximizingPlayer);
            board.unmakeMove(move);

            if (stopSearch) return best;

            if (maximizingPlayer) {
                if (score > best) {
                    best = score;
                    bestMove = move;
                }
                alpha = std::max(alpha, best);
            } else {
                if (score < best) {
                    best = score;
                    bestMove = move;
                }
                beta = std::min(beta, best);
            }

            if (beta <= alpha) break;
        }

        if (!stopSearch) {
            TranspositionTable::Flag flag = TranspositionTable::EXACT;
            if (best <= alphaOrig) flag = TranspositionTable::UPPERBOUND;
            else if (best >= betaOrig) flag = TranspositionTable::LOWERBOUND;

            tt.store(key, depth, scoreToTT(best, ply), flag, bestMove);
        }

        return best;
    }

    Move search() {
        searchStart = std::chrono::steady_clock::now();
        stopSearch = false;

        Move lastCompletedBestMove = Move::NULL_MOVE;
        bool maximizingPlayer = (board.sideToMove() == Color::WHITE);

        for (int currentDepth = 1; currentDepth <= 64; currentDepth++) {
            Movelist moves;
            movegen::legalmoves(moves, board);
            orderMoves(moves);

            int bestScore = maximizingPlayer ? -INF : INF;
            Move depthBestMove = Move::NULL_MOVE;

            for (const Move &move : moves) {
                if (stopSearch) break;

                board.makeMove(move);
                int score = minimax(currentDepth - 1, 1, -INF, INF, !maximizingPlayer);
                board.unmakeMove(move);

                if (maximizingPlayer) {
                    if (score > bestScore) {
                        bestScore = score;
                        depthBestMove = move;
                    }
                } else {
                    if (score < bestScore) {
                        bestScore = score;
                        depthBestMove = move;
                    }
                }
            }

            if (stopSearch) break;

            lastCompletedBestMove = depthBestMove;

            if (std::abs(bestScore) >= MATE_SCORE - 100) {
                break;
            }
        }

        return lastCompletedBestMove;
    }

    void parsePosition(const std::string &command) {
        std::stringstream ss(command);
        std::string token;

        ss >> token;
        ss >> token;

        if (token == "startpos") {
            board.setFen(constants::STARTPOS);
        } else if (token == "fen") {
            std::string fen;
            for (int i = 0; i < 6; i++) {
                ss >> token;
                fen += token + (i != 5 ? " " : "");
            }
            board.setFen(fen);
        }

        if (ss >> token && token == "moves") {
            while (ss >> token) {
                Move move = uci::uciToMove(board, token);
                board.makeMove(move);
            }
        }
    }

    void parseGo(const std::string &command) {
        std::stringstream ss(command);
        std::string token;
        ss >> token; 

        while (ss >> token) {
            if (token == "wtime") ss >> wtime;
            else if (token == "btime") ss >> btime;
            else if (token == "winc")  ss >> winc;
            else if (token == "binc")  ss >> binc;
        }
    }
};

int main() {
    Engine engine;
    engine.run();
    return 0;
}
