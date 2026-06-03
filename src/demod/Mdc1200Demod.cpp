#include "demod/Mdc1200Demod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// MDC-1200: 2-FSK, 1200 baud, ±1200 Hz deviation, pre-tone at 1200 Hz
static constexpr double MDC_BAUD = 1200.0;
static constexpr double MDC_DEV  = 1200.0;
static constexpr uint16_t MDC_SYNC = 0x6969;

static const char* mdc_opcode(uint8_t op) {
    switch (op) {
        case 0x00: return "PTT ID";
        case 0x01: return "Emergency";
        case 0x02: return "Emergency Ack";
        case 0x03: return "Call Alert";
        case 0x04: return "Status";
        case 0x20: return "Radio Check";
        case 0x23: return "Remote Monitor";
        case 0x2B: return "Radio Inhibit";
        case 0x2C: return "Radio Uninhibit";
        default:   return "Unknown";
    }
}

// CRC-16 for MDC-1200
static uint16_t mdc_crc(const uint8_t* data, int len) {
    uint16_t crc = 0x0000;
    for (int i=0;i<len;++i) {
        crc ^= static_cast<uint16_t>(data[i])<<8;
        for (int j=0;j<8;++j)
            crc = (crc&0x8000)?(crc<<1)^0x1021:(crc<<1);
    }
    return crc;
}

DemodResult Mdc1200Demod::process(const std::vector<std::complex<float>>& iq,
                                   au::QuantityD<au::Hertz>   sr,
                                   au::QuantityD<au::Hertz>   center_freq,
                                   au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "MDC-1200";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(MDC_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps = static_cast<float>(sr.in(au::hertz)/MDC_BAUD);
    freqdem fdem = freqdem_create(static_cast<float>(MDC_DEV/MDC_BAUD));
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC,static_cast<unsigned>(std::round(sps)),5,0.2f,32);
    symsync_rrrf_set_lf_bw(sync,0.005f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for (const auto& s:iq) {
        float fd; freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for(unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);
    }
    freqdem_destroy(fdem); symsync_rrrf_destroy(sync);

    // Hunt for MDC-1200 sync word 0x6969 (little-endian)
    uint16_t shift=0;
    for (size_t i=0;i<bits.size();++i) {
        shift=(shift>>1)|(bits[i]<<15);
        if (shift==MDC_SYNC && i+80<bits.size()) {
            // 7 data bytes after sync
            uint8_t data[7]={};
            for(int b=0;b<7;++b)
                for(int k=0;k<8;++k)
                    data[b]|=static_cast<uint8_t>(bits[i+1+b*8+k]<<k);
            uint8_t  op     = data[0];
            uint8_t  arg    = data[1];
            uint16_t unitid = static_cast<uint16_t>(data[2]|(data[3]<<8));
            spdlog::info("MDC-1200: op=0x{:02X} ({}) unit={} arg=0x{:02X}",
                         op, mdc_opcode(op), unitid, arg);
            std::string txt = std::string(mdc_opcode(op)) +
                              " UNIT:" + std::to_string(unitid);
            r.bits.assign(txt.begin(),txt.end());
            return r;
        }
    }

    // Fall back to raw bytes
    r.bits.reserve((bits.size()+7)/8);
    for(size_t i=0;i+8<=bits.size();i+=8) {
        uint8_t byte=0;
        for(int b=0;b<8;++b) byte|=static_cast<uint8_t>(bits[i+b]<<b);
        r.bits.push_back(byte);
    }
    return r;
}

} // namespace demod
