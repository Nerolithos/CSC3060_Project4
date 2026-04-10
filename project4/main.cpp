#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "memory_hierarchy.h"

using namespace std;

namespace {

void printUsage(const char* argv0) {
    cerr << "Usage: " << argv0
         << " <trace> <L1_KB> <assoc> <block> <L1_lat> <Mem_lat>"
         << " [L1_policy] [L1_prefetcher] [--enable-l2 [L2_policy] [L2_prefetcher]]" << endl;
    cerr << "  Omit --enable-l2 for Task 1 (L1 -> Memory)." << endl;
    cerr << "  Add  --enable-l2 for Task 2/3 multi-level runs (L1 -> L2 -> Memory)." << endl;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h")) {
        printUsage(argv[0]);
        return 0;
    }

    if (argc < 7) {
        printUsage(argv[0]);
        return 1;
    }

    string trace_file = argv[1];
    vector<string> extra_args;
    for (int i = 7; i < argc; ++i) {
        extra_args.push_back(argv[i]);
    }

    bool enable_l2 = false;
    vector<string> positional_args;
    for (const string& arg : extra_args) {
        if (arg == "--enable-l2") {
            enable_l2 = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg.rfind("--", 0) == 0) {
            cerr << "Error: Unknown option " << arg << endl;
            printUsage(argv[0]);
            return 1;
        }
        positional_args.push_back(arg);
    }

    if (!enable_l2 && positional_args.size() > 2) {
        cerr << "Error: L2 policy/prefetcher arguments require --enable-l2." << endl;
        printUsage(argv[0]);
        return 1;
    }
    if (positional_args.size() > 4) {
        cerr << "Error: Too many positional arguments." << endl;
        printUsage(argv[0]);
        return 1;
    }

    CacheConfig l1_cfg;
    l1_cfg.size_kb = stoi(argv[2]);
    l1_cfg.associativity = stoi(argv[3]);
    l1_cfg.block_size = stoi(argv[4]);
    l1_cfg.latency = stoi(argv[5]);
    l1_cfg.policy_name = positional_args.size() > 0 ? positional_args[0] : "LRU";
    l1_cfg.prefetcher = positional_args.size() > 1 ? positional_args[1] : "None";

    int mem_lat = stoi(argv[6]);

    MainMemory* memory = new MainMemory(mem_lat);

    CacheLevel* l2 = nullptr;
    if (enable_l2) {
        CacheConfig l2_cfg;
        l2_cfg.size_kb = l1_cfg.size_kb * 4;
        l2_cfg.associativity = l1_cfg.associativity;
        l2_cfg.block_size = l1_cfg.block_size;
        l2_cfg.latency = l1_cfg.latency * 4;
        l2_cfg.policy_name = positional_args.size() > 2 ? positional_args[2] : l1_cfg.policy_name;
        l2_cfg.prefetcher = positional_args.size() > 3 ? positional_args[3] : "None";
        l2 = new CacheLevel("L2", l2_cfg, memory);
    }

    MemoryObject* l1_next = memory;

    if (enable_l2) {
        l1_next = l2;
    }

    CacheLevel* l1 = new CacheLevel("L1", l1_cfg, l1_next);

    ifstream infile(trace_file);
    if (!infile) {
        cerr << "Error: Could not open trace file " << trace_file << endl;
        delete l1;
        delete l2;
        delete memory;
        return 1;
    }

    cout << "\n=== Starting Simulation ===" << endl;

    string line;
    uint64_t total_cycles = 0;
    uint64_t total_insts = 0;
    uint64_t cycle = 0;

    while (getline(infile, line)) {
        stringstream ss(line);
        char type;
        string addr_str;
        ss >> type >> addr_str;

        if (type != 'r' && type != 'w') {
            continue;
        }

        uint64_t addr = stoull(addr_str, nullptr, 16);
        ++cycle;
        ++total_insts;
        total_cycles += l1->access(addr, type, cycle);
    }

    cout << "\n=== Simulation Results ===" << endl;
    l1->printStats();
    if (l2) l2->printStats();
    memory->printStats();

    cout << "\nMetrics:" << endl;
    cout << "  Total Instructions: " << total_insts << endl;
    cout << "  Total Cycles:       " << total_cycles << endl;
    if (total_insts == 0) {
        cout << "  AMAT:               N/A" << endl;
    } else {
        double amat = static_cast<double>(total_cycles) / total_insts;
        cout << "  AMAT:               " << fixed << setprecision(2) << amat << " cycles" << endl;
    }

    delete l1;
    delete l2;
    delete memory;
    return 0;
}
