# Build environment for the PS4 Linux Launcher (OpenOrbis PS4 Toolchain).
#
# This image contains everything needed to compile the homebrew ELF and build
# the .pkg: clang/lld, make, and the OpenOrbis PS4 Toolchain (create-fself,
# create-gp4, PkgTool.Core). The project source is mounted at build time, so
# the resulting .pkg lands back in the repository on the host.
# Ubuntu 24.04 ships clang/lld 18, matching the LLVM 18 the toolchain targets.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
# PkgTool.Core is a self-contained .NET binary; run it without ICU to avoid
# needing a matching libicu on the host image.
ENV DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
    DOTNET_CLI_TELEMETRY_OPTOUT=1

RUN apt-get update && apt-get install -y --no-install-recommends \
        clang \
        lld \
        make \
        python3 \
        wget \
        ca-certificates \
        tar \
        gzip \
        libicu74 \
        libssl3 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Version of the OpenOrbis PS4 Toolchain to install.
ARG OO_VERSION=v0.5.4
ARG OO_ASSET=toolchain-llvm-18.tar.gz
ENV OO_PS4_TOOLCHAIN=/opt/OpenOrbis/PS4Toolchain

# Download and install the toolchain. The release archive's top-level folder
# name has changed across versions, so we locate the real root by finding the
# create-fself binary and move it to a stable path.
RUN mkdir -p /tmp/oo /opt/OpenOrbis \
    && wget -q "https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/${OO_VERSION}/${OO_ASSET}" -O /tmp/oo.tar.gz \
    && tar xzf /tmp/oo.tar.gz -C /tmp/oo \
    && ROOT="$(dirname "$(dirname "$(dirname "$(find /tmp/oo -type f -name create-fself | head -n1)")")")" \
    && if [ -z "$ROOT" ] || [ ! -d "$ROOT" ]; then echo "Could not locate toolchain root in archive" && ls -R /tmp/oo && exit 1; fi \
    && mv "$ROOT" "$OO_PS4_TOOLCHAIN" \
    && chmod -R +x "$OO_PS4_TOOLCHAIN/bin" \
    && rm -rf /tmp/oo /tmp/oo.tar.gz

# PkgTool.Core is built against an older .NET runtime whose crypto layer needs
# OpenSSL 1.1 (libssl.so.1.1). Ubuntu 24.04 only ships OpenSSL 3, so pull the
# 1.1 runtime from the Ubuntu focal release pool (this exact version is a
# permanent archive artifact and won't disappear).
RUN wget -q "http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2_amd64.deb" -O /tmp/libssl1.1.deb \
    && dpkg -i /tmp/libssl1.1.deb \
    && rm /tmp/libssl1.1.deb

WORKDIR /project

CMD ["bash", "-lc", "make clean; make"]
