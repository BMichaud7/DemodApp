# ── Stage 1: build ────────────────────────────────────────────────────────────
# Build context: /home/brendan  (podman build -f DemodApp/Containerfile -t sdr-demod:1.1.0 .)
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config ca-certificates \
        libliquid-dev \
        libqpid-proton-cpp12-dev \
        libtinyxml2-dev \
        libspdlog-dev \
        libfmt-dev \
        libfftw3-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone SdrSdk (brings in Au via FetchContent) and SdrTaskApi as sibling deps
RUN git clone --depth 1 --branch "main/1.0" \
        https://github.com/BMichaud7/SdrSdk.git /build/SdrSdk && \
    git clone --depth 1 --branch "main/1.0" \
        https://github.com/BMichaud7/SdrTaskApi.git /build/SdrTaskApi

COPY DemodApp/ /build/DemodApp/

WORKDIR /build/DemodApp
RUN cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/install \
        -DBUILD_TESTING=OFF \
        -DFETCHCONTENT_QUIET=OFF \
    && cmake --build build --parallel "$(nproc)" \
    && cmake --install build

# ── Stage: test (run with --target test) ─────────────────────────────────────
# podman build --target test -t sdr-demod:test -f DemodApp/Containerfile .
# podman run --rm sdr-demod:test
FROM builder AS test
RUN cmake -B build_test \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=ON \
        -DFETCHCONTENT_QUIET=OFF \
    && cmake --build build_test --parallel "$(nproc)" --target demod_tests
RUN ctest --test-dir build_test --output-on-failure -V --timeout 120

# ── Stage 2: runtime ──────────────────────────────────────────────────────────
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        libliquid1 \
        libqpid-proton-cpp12 \
        libtinyxml2-10 \
        libspdlog1.12 \
        libfmt9 \
        tini \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/ /usr/local/
RUN echo /usr/local/lib > /etc/ld.so.conf.d/local.conf && ldconfig

RUN mkdir -p /etc/sdr-demod /tmp/sdr-demod
COPY --from=builder /build/DemodApp/config/demod.xml /etc/sdr-demod/demod.xml

ENV SDR_LOG_LEVEL=info
ENV BROKER_URL=amqp://localhost:5672
ENV LOCAL_IP=127.0.0.1
ENV OUTPUT_DIR=/tmp/sdr-demod

VOLUME ["/demod-output"]

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["/usr/local/bin/sdr_demod", "/etc/sdr-demod/demod.xml"]
