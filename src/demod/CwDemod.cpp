#include "demod/CwDemod.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <string>
#include <vector>
#include "au/units/hertz.hh"
#include "au/units/seconds.hh"

namespace demod {

// ---------------------------------------------------------------------------
// Morse table: code string → ASCII character
// Dots and dashes as '.' and '-'.
// ---------------------------------------------------------------------------
static const std::map<std::string, char> k_morse_table = {
    {".-",   'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..",  'D'},
    {".",    'E'}, {"..-.", 'F'}, {"--.",  'G'}, {"....", 'H'},
    {"..",   'I'}, {".---", 'J'}, {"-.-",  'K'}, {".-..", 'L'},
    {"--",   'M'}, {"-.",   'N'}, {"---",  'O'}, {".--.", 'P'},
    {"--.-", 'Q'}, {".-.",  'R'}, {"...",  'S'}, {"-",    'T'},
    {"..-",  'U'}, {"...-", 'V'}, {".--",  'W'}, {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {"-----",'0'}, {".----",'1'}, {"..---",'2'}, {"...--",'3'},
    {"....-",'4'}, {".....", '5'}, {"-....", '6'},
    {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
    {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'},
    {"-...-",  '='}, {".-...", '&'}, {"-.-.-.",  ';'},
    {"-.--.", '('}, {"-.--.-", ')'}, {"---...",  ':'},
    {".----.", '\''}, {"-....-", '-'}, {"-..-.", '/'},
};

// Helper: decode one Morse symbol string → ASCII (0 if unknown)
static char decodeMorse(const std::string& sym) {
    auto it = k_morse_table.find(sym);
    return it != k_morse_table.end() ? it->second : '?';
}

// Helper: integer median (modifies copy)
static int median(std::vector<int> v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// ---------------------------------------------------------------------------

CwDemod::CwDemod() = default;
CwDemod::~CwDemod() = default;

DemodResult CwDemod::process(const std::vector<std::complex<float>>& iq,
                              au::QuantityD<au::Hertz>   sr,
                              au::QuantityD<au::Hertz>   center_freq,
                              au::QuantityD<au::Seconds> timestamp)
{
    // Extract raw values for DSP math
    const double sr_sps         = sr.in(au::hertz);
    const double center_freq_hz = center_freq.in(au::hertz);

    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "CW";
    r.center_freq = center_freq;
    r.sample_rate = sr;
    r.timestamp_ms = static_cast<int64_t>(timestamp.in(au::seconds) * 1000.0);
    r.duration    = au::seconds(iq.size() / sr_sps);

    if (iq.empty()) return r;

    // ── Step 1: envelope ────────────────────────────────────────────────────
    const size_t N = iq.size();
    std::vector<float> env(N);
    for (size_t i = 0; i < N; ++i)
        env[i] = std::abs(iq[i]);

    // ── Step 2: IIR lowpass (α ≈ 0.02) ─────────────────────────────────────
    constexpr float alpha = 0.02f;
    std::vector<float> smooth(N);
    smooth[0] = alpha * env[0];
    for (size_t i = 1; i < N; ++i)
        smooth[i] = alpha * env[i] + (1.0f - alpha) * smooth[i - 1];

    // ── Step 3: threshold ───────────────────────────────────────────────────
    float max_env = *std::max_element(smooth.begin(), smooth.end());
    if (max_env < 1e-6f) {
        spdlog::info("CwDemod: no signal detected");
        return r;
    }
    float threshold = max_env * 0.4f;

    // ── Step 4: binary key_down sequence ────────────────────────────────────
    std::vector<uint8_t> key(N);
    for (size_t i = 0; i < N; ++i)
        key[i] = (smooth[i] > threshold) ? 1 : 0;

    // ── Step 5: run-length encode ────────────────────────────────────────────
    // Each run: {is_mark, length_in_samples}
    struct Run { bool mark; int len; };
    std::vector<Run> runs;
    {
        size_t i = 0;
        while (i < N) {
            bool cur = (key[i] == 1);
            size_t j = i + 1;
            while (j < N && (key[j] == 1) == cur) ++j;
            runs.push_back({cur, static_cast<int>(j - i)});
            i = j;
        }
    }

    // ── Step 6: estimate dit_samples ────────────────────────────────────────
    std::vector<int> mark_lengths;
    for (auto& run : runs)
        if (run.mark) mark_lengths.push_back(run.len);

    int dit_samples;
    if (mark_lengths.empty()) {
        spdlog::info("CwDemod: no marks found");
        return r;
    }
    // Use median of shortest 50% of marks as dit estimate
    std::sort(mark_lengths.begin(), mark_lengths.end());
    size_t half = std::max<size_t>(1, mark_lengths.size() / 2);
    std::vector<int> short_marks(mark_lengths.begin(),
                                  mark_lengths.begin() + static_cast<long>(half));
    dit_samples = median(short_marks);
    if (dit_samples < 1) {
        // fallback: 12 WPM → dit = sr/50
        dit_samples = static_cast<int>(sr_sps / 50.0);
    }

    spdlog::debug("CwDemod: dit_samples={} sr={:.0f}", dit_samples, sr_sps);

    // ── Steps 7–8: classify runs and decode ─────────────────────────────────
    std::string result_text;
    std::string current_symbol; // dits/dahs for current letter

    auto flush_letter = [&]() {
        if (!current_symbol.empty()) {
            result_text += decodeMorse(current_symbol);
            current_symbol.clear();
        }
    };

    for (auto& run : runs) {
        if (run.mark) {
            // Mark: dit or dah
            if (run.len < 2 * dit_samples)
                current_symbol += '.';
            else
                current_symbol += '-';
        } else {
            // Space: intra-element, letter sep, or word sep
            if (run.len < 2 * dit_samples) {
                // intra-element gap — ignore
            } else if (run.len < 4 * dit_samples) {
                // letter separator
                flush_letter();
            } else {
                // word separator
                flush_letter();
                result_text += ' ';
            }
        }
    }
    flush_letter(); // flush last letter

    // Trim trailing space
    while (!result_text.empty() && result_text.back() == ' ')
        result_text.pop_back();

    r.bits.assign(result_text.begin(), result_text.end());

    spdlog::info("CwDemod: {:.3f} MHz → \"{}\"",
                 center_freq_hz / 1e6, result_text);
    return r;
}

} // namespace demod
