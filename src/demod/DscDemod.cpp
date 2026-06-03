#include "demod/DscDemod.hpp"
#include <liquid/liquid.h>
#include <cmath>
#include <spdlog/spdlog.h>

namespace demod {

// DSC Digital Selective Calling: FSK, 1200 baud, ±400 Hz, ITU-R M.493
static constexpr double DSC_BAUD = 1200.0;
static constexpr double DSC_DEV  = 400.0;

static const char* dsc_category(uint8_t c){
    switch(c){
        case 112: return "Distress";
        case 108: return "Urgency";
        case 102: return "Safety";
        case 100: return "Routine";
        default:  return "Unknown";
    }
}

DemodResult DscDemod::process(const std::vector<std::complex<float>>& iq,
                               au::QuantityD<au::Hertz>   sr,
                               au::QuantityD<au::Hertz>   center_freq,
                               au::QuantityD<au::Seconds> timestamp) {
    DemodResult r;
    r.type        = DemodClass::Bits;
    r.modulation  = "DSC";
    r.center_freq = center_freq;
    r.sample_rate = au::hertz(DSC_BAUD);
    r.timestamp_ms= static_cast<int64_t>(timestamp.in(au::seconds)*1000);
    r.duration    = sr.in(au::hertz)>0?au::seconds(iq.size()/sr.in(au::hertz)):au::seconds(0.0);
    if (iq.empty()) return r;

    float sps=static_cast<float>(sr.in(au::hertz)/DSC_BAUD);
    freqdem fdem=freqdem_create(static_cast<float>(DSC_DEV/DSC_BAUD));
    symsync_rrrf sync=symsync_rrrf_create_rnyquist(
        LIQUID_FIRFILT_RRC,static_cast<unsigned>(std::round(sps)),5,0.4f,32);
    symsync_rrrf_set_lf_bw(sync,0.01f);

    std::vector<uint8_t> bits;
    float buf[4]; unsigned n;
    for(const auto& s:iq){float fd;freqdem_demodulate(fdem,s,&fd);
        symsync_rrrf_execute(sync,&fd,1,buf,&n);
        for(unsigned i=0;i<n;++i) bits.push_back(buf[i]>0.0f?1:0);}
    freqdem_destroy(fdem);symsync_rrrf_destroy(sync);

    // DSC uses 7-bit code with 1 ECC bit (10-bit symbol with 3 DX bits)
    // Simplified: extract bytes and look for DSC phasing sequence 0x7B7B...
    std::vector<uint8_t> bytes;
    for(size_t i=0;i+7<=(size_t)bits.size();i+=7){
        uint8_t byte=0;
        for(int b=0;b<7;++b) byte|=static_cast<uint8_t>(bits[i+b]<<(6-b));
        bytes.push_back(byte);
    }

    // Find DSC phasing sequence and extract MMSI
    for(size_t i=0;i+20<(size_t)bytes.size();++i){
        if(bytes[i]==0x7B&&bytes[i+1]==0x7B){
            // Category is in position i+3
            uint8_t cat=bytes[i+3]&0x7F;
            // MMSI is 9 digits in positions i+5..i+9 (BCD encoded)
            std::string mmsi;
            for(int m=0;m<5&&i+5+m<bytes.size();++m){
                mmsi+=std::to_string((bytes[i+5+m]>>4)&0xF);
                mmsi+=std::to_string( bytes[i+5+m]     &0xF);
            }
            std::string msg=std::string(dsc_category(cat))+" MMSI:"+mmsi.substr(0,9);
            spdlog::info("DSC: {}",msg);
            r.bits.assign(msg.begin(),msg.end());
            return r;
        }
    }
    r.bits=bytes;
    return r;
}

} // namespace demod
