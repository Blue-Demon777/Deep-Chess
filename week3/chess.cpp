#include "chess.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace chess;

class FlatJson {
   public:
    static std::vector<std::pair<std::string, std::string>> parse(const std::string& text) {
        std::vector<std::pair<std::string, std::string>> out;
        std::size_t i = 0, n = text.size();

        auto skipWs = [&]() {
            while (i < n && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
        };

        auto parseString = [&]() -> std::string {
            std::string s;
            ++i; 
            while (i < n && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < n) {
                    char c = text[i + 1];
                    switch (c) {
                        case 'n': s += '\n'; break;
                        case 't': s += '\t'; break;
                        case 'r': s += '\r'; break;
                        case '"': s += '"'; break;
                        case '\\': s += '\\'; break;
                        case '/': s += '/'; break;
                        default: s += c; break;
                    }
                    i += 2;
                } else {
                    s += text[i++];
                }
            }
            if (i < n) ++i; 
            return s;
        };

        skipWs();
        if (i < n && text[i] == '{') ++i;

        while (true) {
            skipWs();
            if (i >= n || text[i] == '}') break;
            if (text[i] == ',') {
                ++i;
                continue;
            }
            if (text[i] != '"') break; 

            std::string key = parseString();
            skipWs();
            if (i < n && text[i] == ':') ++i;
            skipWs();
            std::string value = parseString();

            out.emplace_back(std::move(key), std::move(value));
        }

        return out;
    }
};

class MateEngine {
   public:
    struct SolveResult {
        bool found = false;
        std::vector<Move> pv;
        long long nodes = 0;
        bool nodeLimitHit = false;
    };

    explicit MateEngine(long long nodeLimit = 50'000'000) : nodeLimit_(nodeLimit) {}

    SolveResult solve(Board board, int mateInMoves) {
        nodes_ = 0;
        limitHit_ = false;

        SolveResult result;
        std::vector<Move> line;
        result.found = attackerSearch(board, mateInMoves, line);
        result.pv = std::move(line);
        result.nodes = nodes_;
        result.nodeLimitHit = limitHit_;
        return result;
    }

   private:
    long long nodeLimit_;
    long long nodes_ = 0;
    bool limitHit_ = false;

    static bool isCheckmate(Board& b) {
        Movelist moves;
        movegen::legalmoves(moves, b);
        return moves.empty() && b.inCheck();
    }
    static int moveScore(const Board& b, const Move& m) {
        int score = 0;
        if (b.givesCheck(m) != CheckType::NO_CHECK) score += 10'000;
        if (b.isCapture(m)) {
            PieceType captured = b.getCapturing<PieceType>(m);
            PieceType attacker = b.at<PieceType>(m.from());
            score += 1'000 + 10 * pieceValue(captured) - pieceValue(attacker);
        }
        if (m.typeOf() == Move::PROMOTION) score += 800 + pieceValue(m.promotionType());
        return score;
    }

    static int pieceValue(PieceType pt) {
        switch (pt.internal()) {
            case PieceType::PAWN: return 1;
            case PieceType::KNIGHT: return 3;
            case PieceType::BISHOP: return 3;
            case PieceType::ROOK: return 5;
            case PieceType::QUEEN: return 9;
            default: return 0;
        }
    }

    static void orderMoves(const Board& b, Movelist& moves) {
        std::sort(moves.begin(), moves.end(),
                  [&](const Move& a, const Move& c) { return moveScore(b, a) > moveScore(b, c); });
    }

    bool attackerSearch(Board& b, int movesLeft, std::vector<Move>& line) {
        if (++nodes_ > nodeLimit_) {
            limitHit_ = true;
            return false;
        }

        Movelist moves;
        movegen::legalmoves(moves, b);

        if (movesLeft == 1) {
            Movelist checks;
            for (const auto& m : moves)
                if (b.givesCheck(m) != CheckType::NO_CHECK) checks.add(m);
            moves = checks;
        }

        orderMoves(b, moves);

        for (const auto& m : moves) {
            b.makeMove(m);
            line.push_back(m);

            bool ok = (movesLeft == 1) ? isCheckmate(b) : defenderSearch(b, movesLeft - 1, line);

            b.unmakeMove(m);

            if (ok) return true;
            line.pop_back();

            if (limitHit_) return false;
        }
        return false;
    }

    bool defenderSearch(Board& b, int movesLeft, std::vector<Move>& line) {
        if (++nodes_ > nodeLimit_) {
            limitHit_ = true;
            return false;
        }

        Movelist moves;
        movegen::legalmoves(moves, b);

        if (moves.empty()) {
            return b.inCheck();
        }

        orderMoves(b, moves);

        const std::size_t baseSize = line.size();
        std::vector<Move> winningContinuation; 

        for (const auto& m : moves) {
            b.makeMove(m);
            line.push_back(m);

            bool ok = attackerSearch(b, movesLeft, line);

            b.unmakeMove(m);

            if (!ok) {
                line.resize(baseSize);
                return false;
            }

            if (winningContinuation.empty()) {
                winningContinuation.assign(line.begin() + baseSize, line.end());
            }
            line.resize(baseSize); 

            if (limitHit_) return false;
        }

        line.insert(line.end(), winningContinuation.begin(), winningContinuation.end());
        return true;
    }
};

// =====================================================================================
//  Helpers   -- claude power
// =====================================================================================
static std::string formatPV(Board board, const std::vector<Move>& pv) {
    std::ostringstream oss;
    int moveNum = 1;
    bool whiteToMove = board.sideToMove() == Color::WHITE;
    bool first = true;

    for (const auto& m : pv) {
        std::string san = uci::moveToSan(board, m);

        if (whiteToMove) {
            oss << moveNum << ". " << san << ' ';
        } else if (first) {
            oss << moveNum << "... " << san << ' ';
        } else {
            oss << san << ' ';
        }

        board.makeMove(m);

        if (!whiteToMove) ++moveNum;
        whiteToMove = !whiteToMove;
        first = false;
    }

    std::string s = oss.str();
    if (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

struct Puzzle {
    std::string fen;
    std::string expected;
    int mateIn = 0;
    std::string source; 
};

struct PuzzleOutcome {
    bool solved = false;
    bool nodeLimitHit = false;
    std::string foundLine;
    long long nodes = 0;
    double seconds = 0.0;
};

static int inferMateLength(const std::string& path) {
    std::string base = path;
    auto slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);

    for (std::size_t i = 0; i < base.size(); ++i) {
        if ((base[i] == 'n' || base[i] == 'N') && i + 1 < base.size() &&
            std::isdigit(static_cast<unsigned char>(base[i + 1]))) {
            std::string digits;
            std::size_t j = i + 1;
            while (j < base.size() && std::isdigit(static_cast<unsigned char>(base[j]))) digits += base[j++];
            return std::stoi(digits);
        }
    }
    return 0;
}

static std::vector<Puzzle> loadPuzzleFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open " << path << "\n";
        return {};
    }
    std::ostringstream buf;
    buf << in.rdbuf();

    int mateIn = inferMateLength(path);
    if (mateIn <= 0) {
        std::cerr << "Could not infer mate length from filename '" << path
                  << "' (expected something like m8n3.json) -- skipping.\n";
        return {};
    }

    auto entries = FlatJson::parse(buf.str());
    std::vector<Puzzle> puzzles;
    puzzles.reserve(entries.size());
    for (auto& [fen, sol] : entries) {
        puzzles.push_back(Puzzle{fen, sol, mateIn, path});
    }
    return puzzles;
}

// =====================================================================================
//  Driver: runs every puzzle, in parallel, and reports a summary.
// =====================================================================================
class PuzzleRunner {
   public:
    PuzzleRunner(std::vector<Puzzle> puzzles, unsigned threadCount, bool verbose)
        : puzzles_(std::move(puzzles)), threadCount_(threadCount), verbose_(verbose) {
        outcomes_.resize(puzzles_.size());
    }

    void run() {
        std::atomic<std::size_t> nextIndex{0};
        std::atomic<std::size_t> completed{0};
        auto t0 = std::chrono::steady_clock::now();

        auto worker = [&]() {
            MateEngine engine;
            while (true) {
                std::size_t idx = nextIndex.fetch_add(1);
                if (idx >= puzzles_.size()) break;

                const Puzzle& p = puzzles_[idx];
                Board board(p.fen);

                auto start = std::chrono::steady_clock::now();
                auto res = engine.solve(board, p.mateIn);
                auto end = std::chrono::steady_clock::now();

                PuzzleOutcome out;
                out.solved = res.found;
                out.nodeLimitHit = res.nodeLimitHit;
                out.nodes = res.nodes;
                out.seconds = std::chrono::duration<double>(end - start).count();
                if (res.found) out.foundLine = formatPV(board, res.pv);

                outcomes_[idx] = std::move(out);
                completed.fetch_add(1);
            }
        };

        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threadCount_; ++t) pool.emplace_back(worker);

        std::size_t total = puzzles_.size();
        while (completed.load() < total) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            std::cerr << "\rSolving... " << completed.load() << " / " << total << std::flush;
        }
        for (auto& th : pool) th.join();
        std::cerr << "\rSolving... " << total << " / " << total << "\n";

        auto t1 = std::chrono::steady_clock::now();
        wallSeconds_ = std::chrono::duration<double>(t1 - t0).count();

        report();
    }

   private:
    std::vector<Puzzle> puzzles_;
    std::vector<PuzzleOutcome> outcomes_;
    unsigned threadCount_;
    bool verbose_;
    double wallSeconds_ = 0.0;

    void report() const {
        std::cout << "\n================ Results ================\n";

        std::vector<std::string> files;
        for (auto& p : puzzles_)
            if (std::find(files.begin(), files.end(), p.source) == files.end()) files.push_back(p.source);

        long long grandSolved = 0, grandTotal = 0, grandNodes = 0;
        double grandTime = 0.0;

        for (auto& file : files) {
            long long solved = 0, total = 0, nodes = 0, limitHit = 0;
            double time = 0.0;

            for (std::size_t i = 0; i < puzzles_.size(); ++i) {
                if (puzzles_[i].source != file) continue;
                ++total;
                nodes += outcomes_[i].nodes;
                time += outcomes_[i].seconds;
                if (outcomes_[i].solved) ++solved;
                if (outcomes_[i].nodeLimitHit) ++limitHit;

                if (verbose_) {
                    std::cout << (outcomes_[i].solved ? "[OK]   " : "[FAIL] ") << file << "  mate-in-"
                              << puzzles_[i].mateIn << "  " << puzzles_[i].fen << "\n";
                    if (outcomes_[i].solved) {
                        std::cout << "       found:    " << outcomes_[i].foundLine << "\n";
                        std::cout << "       expected: " << puzzles_[i].expected << "\n";
                    } else {
                        std::cout << "       expected: " << puzzles_[i].expected
                                   << (outcomes_[i].nodeLimitHit ? "   (node limit hit)" : "") << "\n";
                    }
                }
            }

            std::cout << file << ":  " << solved << " / " << total << " solved";
            if (limitHit) std::cout << "  (" << limitHit << " hit the node limit)";
            std::cout << "\n  total nodes: " << nodes << "   total cpu time: " << time
                       << "s   avg: " << (total ? time / total * 1000.0 : 0.0) << "ms/puzzle\n\n";

            grandSolved += solved;
            grandTotal += total;
            grandNodes += nodes;
            grandTime += time;
        }

        std::cout << "-------------------------------------------\n";
        std::cout << "TOTAL: " << grandSolved << " / " << grandTotal << " solved\n";
        std::cout << "Total search nodes: " << grandNodes << "\n";
        std::cout << "Total CPU time (summed across threads): " << grandTime << "s\n";
        std::cout << "Wall-clock time (" << threadCount_ << " threads): " << wallSeconds_ << "s\n";
        if (wallSeconds_ > 0)
            std::cout << "Throughput: " << (grandTotal / wallSeconds_) << " puzzles/sec\n";
    }
};

// =====================================================================================
//  main
// =====================================================================================
int main(int argc, char** argv) {
    std::vector<std::string> files;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    bool verbose = false;
    int limit = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--threads" && i + 1 < argc) {
            threads = static_cast<unsigned>(std::stoi(argv[++i]));
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::stoi(argv[++i]);
        } else {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        files = {"m8n2.json", "m8n3.json", "m8n4.json"};
    }

    std::vector<Puzzle> all;
    for (auto& f : files) {
        auto puzzles = loadPuzzleFile(f);
        if (limit >= 0 && static_cast<int>(puzzles.size()) > limit) puzzles.resize(limit);
        std::cout << "Loaded " << puzzles.size() << " puzzles from " << f << "\n";
        all.insert(all.end(), puzzles.begin(), puzzles.end());
    }

    if (all.empty()) {
        std::cerr << "No puzzles loaded -- nothing to solve.\n";
        return 1;
    }

    std::cout << "Solving " << all.size() << " puzzles using " << threads << " threads...\n";

    PuzzleRunner runner(std::move(all), threads, verbose);
    runner.run();
    
    return 0;
}
