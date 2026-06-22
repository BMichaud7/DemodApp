# DemodApp — RF Signal Demodulation Service

Subscribes to analysis results from AnalysisApp (`rf.analysis` topic), fetches raw IQ from the SDR controller, demodulates classified signals using modulation-specific algorithms, and publishes decoded output to `rf.demod`.

## Architecture

```
AnalysisApp ──► rf.analysis (AMQP) ──► DemodService
                                            │
                                            ▼
                                       DemodRouter
                                      (confidence + cooldown gate)
                                            │
                    ┌───────────────────────┼─────────────────────────┐
                    ▼                       ▼                         ▼
               IqFetcher          paramsFor(modulation)        (filters skipped)
          (NARROWBAND task)              │
                    │              ┌─────┴────────────────────────────────────┐
                    │              │  FmDemod  AmDemod  AfskDemod  CwDemod    │
                    └──────────────►  FskDemod PskQamDemod                    │
                                   └────────────────────────────────────┬─────┘
                                                                         │
                                        rf.demod (AMQP) ◄───────────────┘
                                        /tmp/sdr-demod/  (optional file output)
```

## Demodulators

| Class | Modulations | Backend |
|---|---|---|
| `FmDemod` | FM_NB, FM_WB | liquid-dsp `freqdem` |
| `AmDemod` | AM_DSB, AM_DSB_SC, AM_SSB_USB, AM_SSB_LSB | liquid-dsp `ampmodem` |
| `AfskDemod` | AFSK, APRS | liquid-dsp `fskdem` |
| `CwDemod` | CW | liquid-dsp envelope + Goertzel tone |
| `FskDemod` | FSK, 4FSK, 8FSK, GFSK, GMSK, MSK | liquid-dsp `fskdem` |
| `PskQamDemod` | BPSK, QPSK, 8PSK, QAM16/32/64/256, 4ASK, 16ASK | liquid-dsp `modem` + Costas-loop |

All frequency, bandwidth, and symbol-rate parameters use [Au units](https://github.com/aurora-opensource/au) (`au::QuantityD<au::Hertz>`) throughout — unit mismatches are compile-time errors.

## AMQP I/O

| Direction | Queue / Topic | Schema |
|---|---|---|
| Subscribe | `rf.demod.request` (queue) | `DEMOD_REQUEST` JSON |
| Publish | `rf.demod` (topic) | `DEMOD_RESULT` JSON |
| IQ fetch | `sdr.task.request` (queue) | SdrTaskApi `NARROWBAND` task |

## Configuration

```xml
<demod_config version="1.0">
  <broker>
    <url>amqp://localhost:5672</url>
    <username>sdr_ctrl</username>
    <password>sdr_hw_test</password>
    <demod_request_queue>rf.demod.request</demod_request_queue>
    <demod_topic>rf.demod</demod_topic>
    <task_request_queue>sdr.task.request</task_request_queue>
  </broker>
  <engine>
    <rank>4</rank>                          <!-- IQ fetch priority (higher = higher priority) -->
    <audio_duration_sec>5.0</audio_duration_sec>
    <digital_duration_sec>2.0</digital_duration_sec>
    <audio_sample_rate_hz>48000</audio_sample_rate_hz>
  </engine>
  <output>
    <dir>/tmp/sdr-demod</dir>
    <publish_amqp>true</publish_amqp>
  </output>
  <local_ip>127.0.0.1</local_ip>
</demod_config>
```

## Dependencies

| Library | Purpose | Install |
|---|---|---|
| `liquid-dsp` | All demodulators | `apt install libliquid-dev` |
| `libqpid-proton-cpp-dev` | AMQP broker | `apt install libqpid-proton-cpp12-dev` |
| `libtinyxml2-dev` | Config parser | `apt install libtinyxml2-dev` |
| Au units 0.5.1 | Physical units | Auto-fetched by CMake |
| spdlog ≥ 1.14.1 | Logging | Auto-fetched by CMake |
| SdrSdk / SdrTaskApi | IQ fetch | Sibling directory `../SdrSdk` |

## Building

**Container (recommended — liquid-dsp included):**

```bash
podman build -t sdr-demod:1.1.0 .
```

**Native:**

```bash
# liquid-dsp must be installed first
git clone https://github.com/BMichaud7/SdrSdk.git ../SdrSdk

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
```

> **Note:** unit tests require liquid-dsp on the host (`libliquid-dev`). The container build always includes it.

## Running

```bash
podman run -d --name sdr-demod --network=host \
    -v /path/to/demod.xml:/etc/sdr-demod/demod.xml:ro \
    ghcr.io/bmichaud7/sdr-demod:1.1.0
```

Or via `SdrScripts/scan.sh` which manages the full stack lifecycle.

## CI

GitHub Actions workflow (`.github/workflows/ci.yml`) builds on `ubuntu-24.04` with all native deps, runs `cmake --build`, and verifies the binary links cleanly.
