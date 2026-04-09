#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

constexpr uint64_t kBlockSize = 64;
constexpr uint64_t kL1SizeBytes = 32ull * 1024;
constexpr uint64_t kL1Assoc = 8;
constexpr uint64_t kL2SizeBytes = 128ull * 1024;
constexpr uint64_t kL2Assoc = 8;
constexpr uint64_t kL1Sets = kL1SizeBytes / (kBlockSize * kL1Assoc);
constexpr uint64_t kL2Sets = kL2SizeBytes / (kBlockSize * kL2Assoc);
constexpr uint64_t kL1SetSpan = kL1Sets * kBlockSize;
constexpr uint64_t kL2SetSpan = kL2Sets * kBlockSize;
constexpr uint64_t kBaseAddress = 0x10000000ull;

enum class CacheBlend { Variant0, Variant1 };
enum class StreamBlend { Variant0, Variant1 };

struct StudentProfile {
    CacheBlend cache_blend;
    StreamBlend stream_blend;
};

uint64_t alignUp(uint64_t value, uint64_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
}

bool isNumber(const string& text) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (!isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

uint64_t fnv1a64(const string& text) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : text) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

StudentProfile pickProfile(uint64_t seed) {
    switch (seed % 4) {
        case 0: return {CacheBlend::Variant0, StreamBlend::Variant0};
        case 1: return {CacheBlend::Variant0, StreamBlend::Variant1};
        case 2: return {CacheBlend::Variant1, StreamBlend::Variant0};
        default: return {CacheBlend::Variant1, StreamBlend::Variant1};
    }
}

void emitAccess(char type, uint64_t addr) {
    cout << type << " 0x" << hex << addr << dec << '\n';
}

struct TraceBuilder {
    uint64_t cursor = kBaseAddress;

    uint64_t reserve(uint64_t bytes, uint64_t alignment = kBlockSize) {
        cursor = alignUp(cursor, alignment);
        uint64_t base = cursor;
        cursor += alignUp(bytes, kBlockSize) + alignment;
        return base;
    }

    void comment(const string& text) {
        cout << "# " << text << '\n';
    }

    void read(uint64_t addr) {
        emitAccess('r', addr);
    }

    void write(uint64_t addr) {
        emitAccess('w', addr);
    }
};

vector<uint64_t> makeContiguousRegion(TraceBuilder& builder, size_t lines,
                                      uint64_t setSkew = 0) {
    uint64_t base = builder.reserve(lines * kBlockSize + kL2SetSpan, kL2SetSpan) +
                    (setSkew % kL2Sets) * kBlockSize;
    vector<uint64_t> region;
    region.reserve(lines);
    for (size_t i = 0; i < lines; ++i) {
        region.push_back(base + i * kBlockSize);
    }
    return region;
}

vector<uint64_t> makeSameSetRegion(TraceBuilder& builder, size_t lines, uint64_t setIndex) {
    uint64_t base = builder.reserve((lines - 1) * kL1SetSpan + kBlockSize + kL2SetSpan,
                                    kL2SetSpan) +
                    (setIndex % kL1Sets) * kBlockSize;
    vector<uint64_t> region;
    region.reserve(lines);
    for (size_t i = 0; i < lines; ++i) {
        region.push_back(base + i * kL1SetSpan);
    }
    return region;
}

template <typename T>
void rotateBy(vector<T>& data, size_t amount) {
    if (data.empty()) {
        return;
    }
    amount %= data.size();
    rotate(data.begin(), data.begin() + amount, data.end());
}

void emitHotsetPhase(TraceBuilder& builder, const vector<uint64_t>& lines,
                     int warmWrites, int readEpochs, uint64_t rotationSeed) {
    vector<uint64_t> order = lines;
    rotateBy(order, rotationSeed);

    for (int pass = 0; pass < warmWrites; ++pass) {
        for (uint64_t addr : order) {
            builder.write(addr);
        }
    }
    for (int epoch = 0; epoch < readEpochs; ++epoch) {
        for (uint64_t addr : order) {
            builder.read(addr);
        }
    }
}

void emitConflictCycle(TraceBuilder& builder, const vector<uint64_t>& lines, int epochs) {
    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (uint64_t addr : lines) {
            builder.read(addr);
        }
    }
}

void emitWritebackProbe(TraceBuilder& builder, const vector<uint64_t>& lines) {
    for (size_t i = 0; i + 1 < lines.size(); ++i) {
        builder.write(lines[i]);
    }
    builder.write(lines.back());
    builder.read(lines.front());
}

void emitShortReusePhase(TraceBuilder& builder, const vector<uint64_t>& lines, int rounds) {
    vector<uint64_t> hot(lines.begin(), lines.begin() + 8);
    vector<uint64_t> newcomers(lines.begin() + 8, lines.end());

    for (int warm = 0; warm < 2; ++warm) {
        for (uint64_t addr : hot) {
            builder.read(addr);
        }
    }

    for (int round = 0; round < rounds; ++round) {
        uint64_t newcomer = newcomers[round % newcomers.size()];
        builder.read(newcomer);
        for (int i = 0; i < 5; ++i) {
            builder.read(hot[(round + i) % hot.size()]);
        }
        builder.read(newcomer);
        for (int i = 5; i < 8; ++i) {
            builder.read(hot[(round + i) % hot.size()]);
        }
    }
}

void emitBroadPollutionPhase(TraceBuilder& builder, vector<uint64_t> hot,
                             vector<uint64_t> cold, int rounds) {
    for (int round = 0; round < rounds; ++round) {
        builder.read(hot[round % hot.size()]);
        builder.write(hot[(round + 1) % hot.size()]);
        rotateBy(cold, static_cast<size_t>(round));
        for (uint64_t addr : cold) {
            builder.read(addr);
        }
    }
}

void emitReReferencePhase(TraceBuilder& builder, const vector<uint64_t>& lines, int rounds) {
    vector<uint64_t> cohort(lines.begin(), lines.begin() + 4);
    vector<uint64_t> interferers(lines.begin() + 4, lines.end());

    for (int round = 0; round < rounds; ++round) {
        uint64_t first = cohort[round % cohort.size()];
        uint64_t second = cohort[(round + 1) % cohort.size()];

        builder.read(first);
        for (int i = 0; i < 6; ++i) {
            builder.read(interferers[(round + i) % interferers.size()]);
        }
        builder.read(first);

        builder.read(second);
        for (int i = 0; i < 6; ++i) {
            builder.read(interferers[(round + i + 3) % interferers.size()]);
        }
        builder.read(second);
    }
}

void emitSequentialPhase(TraceBuilder& builder, const vector<uint64_t>& stream,
                         const vector<uint64_t>& kernel, int rounds, uint64_t rotationSeed) {
    vector<uint64_t> order = stream;
    rotateBy(order, rotationSeed);

    for (int round = 0; round < rounds; ++round) {
        for (size_t i = 0; i < order.size(); ++i) {
            uint64_t addr = order[i];
            builder.read(addr);
            if ((i % 8) == 0) {
                builder.read(kernel[(i / 8 + round) % kernel.size()]);
            }
            if ((i % 32) == 0) {
                builder.write(addr);
            }
        }
        rotateBy(order, static_cast<size_t>(round + 1));
    }
}

void emitRegularStepPhase(TraceBuilder& builder, const vector<uint64_t>& region,
                          size_t strideBlocks, int rounds, uint64_t rotationSeed) {
    size_t position = rotationSeed % region.size();
    for (int round = 0; round < rounds; ++round) {
        for (size_t i = 0; i < region.size(); ++i) {
            uint64_t addr = region[position];
            builder.read(addr);
            if ((i % 32) == 0) {
                builder.write(addr);
            }
            position = (position + strideBlocks) % region.size();
        }
        position = (position + round + 1) % region.size();
    }
}

void emitWindowReusePhase(TraceBuilder& builder, vector<uint64_t> lines, int passes,
                          bool firstPassWrites, uint64_t rotationSeed) {
    rotateBy(lines, rotationSeed);
    for (int pass = 0; pass < passes; ++pass) {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (firstPassWrites && pass == 0 && (i % 16) == 0) {
                builder.write(lines[i]);
            } else {
                builder.read(lines[i]);
            }
        }
    }
}

void emitBaseTrace() {
    TraceBuilder builder;

    builder.comment("Deterministic validation trace for CSC3060 Project 4");
    builder.comment("Reference geometry: L1=32KB 8-way 64B, L2=128KB 8-way 64B");

    vector<uint64_t> hot = makeContiguousRegion(builder, 4, 5);
    emitHotsetPhase(builder, hot, 1, 3, 0);

    vector<uint64_t> l1Conflict = makeSameSetRegion(builder, 10, 3);
    emitConflictCycle(builder, l1Conflict, 3);

    vector<uint64_t> dirtySet = makeSameSetRegion(builder, 9, 11);
    emitWritebackProbe(builder, dirtySet);
}

void emitStudentTrace(const string& studentId) {
    TraceBuilder builder;
    uint64_t seed = fnv1a64(studentId);
    StudentProfile profile = pickProfile(seed);

    vector<uint64_t> commonHot = makeContiguousRegion(builder, 32, seed % kL2Sets);
    vector<uint64_t> l2Window = makeContiguousRegion(builder, 640, (seed >> 6) % kL2Sets);

    vector<uint64_t> lruRegion = makeSameSetRegion(builder, 10, (seed >> 12) % kL1Sets);
    vector<uint64_t> srripRegion = makeSameSetRegion(builder, 14, (seed >> 16) % kL1Sets);
    vector<uint64_t> bipRegion = makeSameSetRegion(builder, 20, (seed >> 20) % kL1Sets);

    vector<uint64_t> kernel = makeContiguousRegion(builder, 8, (seed >> 24) % kL2Sets);
    vector<uint64_t> nextLineStream = makeContiguousRegion(builder, 512, (seed >> 28) % kL2Sets);
    vector<uint64_t> strideRegion = makeContiguousRegion(builder, 1021, (seed >> 32) % kL2Sets);

    emitHotsetPhase(builder, commonHot, 1, 10, seed % commonHot.size());
    emitWindowReusePhase(builder, l2Window, 3, true, (seed >> 4) % l2Window.size());

    emitShortReusePhase(builder, lruRegion, 24);

    int rerefRounds = (profile.cache_blend == CacheBlend::Variant0) ? 120 : 30;
    int broadRounds = (profile.cache_blend == CacheBlend::Variant1) ? 60 : 15;

    emitReReferencePhase(builder, srripRegion, rerefRounds);
    emitBroadPollutionPhase(builder,
                         vector<uint64_t>(bipRegion.begin(), bipRegion.begin() + 2),
                         vector<uint64_t>(bipRegion.begin() + 2, bipRegion.end()),
                         broadRounds);

    emitHotsetPhase(builder, kernel, 1, 0, 0);

    int sequentialRounds = (profile.stream_blend == StreamBlend::Variant0) ? 3 : 0;
    int stepRounds = (profile.stream_blend == StreamBlend::Variant1) ? 4 : 0;

    emitSequentialPhase(builder, nextLineStream, kernel, sequentialRounds,
                        (seed >> 8) % nextLineStream.size());
    emitRegularStepPhase(builder, strideRegion, 7, stepRounds,
                         (seed >> 10) % strideRegion.size());
}

void emitSingleMode(int mode, uint64_t seed) {
    TraceBuilder builder;

    switch (mode) {
        case 0:
            emitBaseTrace();
            return;
        case 1: {
            vector<uint64_t> region = makeSameSetRegion(builder, 10, seed % kL1Sets);
            emitShortReusePhase(builder, region, 80);
            return;
        }
        case 2: {
            vector<uint64_t> region = makeSameSetRegion(builder, 20, seed % kL1Sets);
            emitBroadPollutionPhase(builder,
                                 vector<uint64_t>(region.begin(), region.begin() + 2),
                                 vector<uint64_t>(region.begin() + 2, region.end()),
                                 60);
            return;
        }
        case 3: {
            vector<uint64_t> region = makeSameSetRegion(builder, 14, seed % kL1Sets);
            emitReReferencePhase(builder, region, 120);
            return;
        }
        case 4: {
            vector<uint64_t> kernel = makeContiguousRegion(builder, 8, (seed >> 4) % kL2Sets);
            vector<uint64_t> stream = makeContiguousRegion(builder, 512, (seed >> 8) % kL2Sets);
            emitHotsetPhase(builder, kernel, 1, 0, 0);
            emitSequentialPhase(builder, stream, kernel, 2, seed % stream.size());
            return;
        }
        case 5: {
            vector<uint64_t> region = makeContiguousRegion(builder, 1021, (seed >> 12) % kL2Sets);
            emitRegularStepPhase(builder, region, 7, 4, seed % region.size());
            return;
        }
        case 6: {
            vector<uint64_t> lines = makeContiguousRegion(builder, 640, (seed >> 16) % kL2Sets);
            emitWindowReusePhase(builder, lines, 3, true, seed % lines.size());
            return;
        }
        default:
            throw runtime_error("Invalid mode");
    }
}

void printUsage(const char* argv0) {
    cerr << "Usage:\n";
    cerr << "  " << argv0 << " base > trace_sanity.txt\n";
    cerr << "  " << argv0 << " student <student_id> > trace_student_<student_id>.txt\n";
    cerr << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printUsage(argv[0]);
            return 1;
        }

        string cmd = argv[1];
        if (cmd == "base") {
            emitBaseTrace();
            return 0;
        }
        if (cmd == "student") {
            if (argc < 3) {
                printUsage(argv[0]);
                return 1;
            }
            emitStudentTrace(argv[2]);
            return 0;
        }
        if (cmd == "mode") {
            if (argc < 3 || !isNumber(argv[2])) {
                printUsage(argv[0]);
                return 1;
            }
            int mode = stoi(argv[2]);
            uint64_t seed = (argc > 3) ? fnv1a64(argv[3]) : 1;
            emitSingleMode(mode, seed);
            return 0;
        }
        if (isNumber(cmd)) {
            int mode = stoi(cmd);
            uint64_t seed = (argc > 2) ? fnv1a64(argv[2]) : 1;
            emitSingleMode(mode, seed);
            return 0;
        }

        printUsage(argv[0]);
        return 1;
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
