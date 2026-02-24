# =============================================================================
# FQCompressor Production Build
# =============================================================================
# 使用 GCC 15 + libstdc++ 进行生产构建
# =============================================================================

# -----------------------------------------------------------------------------
# 构建阶段
# -----------------------------------------------------------------------------
FROM gcc:15.2-bookworm AS build

ARG HTTP_PROXY=""
ARG HTTPS_PROXY=""
ARG ALL_PROXY=""
ARG NO_PROXY="localhost,127.0.0.1"

# 代理仅在构建阶段生效，不持久化到镜像
ENV HTTP_PROXY=${HTTP_PROXY} \
    HTTPS_PROXY=${HTTPS_PROXY} \
    ALL_PROXY=${ALL_PROXY} \
    NO_PROXY=${NO_PROXY} \
    http_proxy=${HTTP_PROXY} \
    https_proxy=${HTTPS_PROXY} \
    all_proxy=${ALL_PROXY} \
    no_proxy=${NO_PROXY}

RUN set -eux; \
    if [ -n "${HTTP_PROXY:-}" ] || [ -n "${HTTPS_PROXY:-}" ]; then \
        : > /etc/apt/apt.conf.d/99proxy; \
        if [ -n "${HTTP_PROXY:-}" ]; then echo "Acquire::http::Proxy \"${HTTP_PROXY}\";" >> /etc/apt/apt.conf.d/99proxy; fi; \
        if [ -n "${HTTPS_PROXY:-}" ]; then echo "Acquire::https::Proxy \"${HTTPS_PROXY}\";" >> /etc/apt/apt.conf.d/99proxy; fi; \
    fi

# APT 镜像源配置 (中国大陆用户可启用国内镜像加速)
ARG USE_CHINA_MIRROR=0
RUN if [ "${USE_CHINA_MIRROR}" = "1" ]; then \
        sed -i 's|deb.debian.org|mirrors.aliyun.com|g' /etc/apt/sources.list.d/debian.sources 2>/dev/null || \
        sed -i 's|deb.debian.org|mirrors.aliyun.com|g' /etc/apt/sources.list 2>/dev/null || true; \
    fi

# APT 重试配置 (处理镜像临时故障)
RUN echo 'Acquire::Retries "3";' > /etc/apt/apt.conf.d/80retries && \
    echo 'Acquire::http::Timeout "30";' >> /etc/apt/apt.conf.d/80retries && \
    echo 'Acquire::https::Timeout "30";' >> /etc/apt/apt.conf.d/80retries

# 安装构建依赖（仅编译所需，不包含开发工具）
RUN apt-get update && apt-get install -y --no-install-recommends --fix-missing \
    build-essential \
    ninja-build \
    pkg-config \
    git \
    wget \
    ca-certificates \
    python3 \
    python3-pip \
    ccache \
    libdeflate-dev \
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libtbb-dev \
    libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

# CMake 4.x (最新版本)
RUN CMAKE_VERSION=4.0.2 && \
    ARCH="$(dpkg --print-architecture)" && \
    case "$ARCH" in \
        amd64) CMAKE_ARCH="x86_64" ;; \
        arm64) CMAKE_ARCH="aarch64" ;; \
        *) echo "Unsupported architecture: $ARCH"; exit 1 ;; \
    esac && \
    wget -q "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}.tar.gz" -O /tmp/cmake.tar.gz && \
    mkdir -p /opt && \
    tar -C /opt -xzf /tmp/cmake.tar.gz && \
    ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}/bin/cmake" /usr/local/bin/cmake && \
    ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}/bin/ctest" /usr/local/bin/ctest && \
    ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}/bin/cpack" /usr/local/bin/cpack && \
    rm -f /tmp/cmake.tar.gz

# Conan 2.x (最新版本)
RUN pip3 install --no-cache-dir --break-system-packages conan==2.24.0

# 设置编译器
ENV CC=gcc
ENV CXX=g++
ENV CONAN_HOME=/root/.conan2
ENV CCACHE_DIR=/root/.ccache
ENV PATH=/usr/lib/ccache:$PATH

WORKDIR /workspace
COPY . .

# 安装依赖并构建
RUN conan profile detect --force
RUN conan install . --build=missing -s build_type=Release -s compiler.cppstd=20 -of=build/gcc-release

RUN cmake -B build/gcc-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=build/gcc-release/conan_toolchain.cmake \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DENABLE_COVERAGE=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX=/opt/fqcompressor

RUN ninja -C build/gcc-release
RUN cmake --install build/gcc-release

# -----------------------------------------------------------------------------
# 生产阶段
# -----------------------------------------------------------------------------
FROM debian:bookworm-slim AS production

# APT 重试配置
RUN echo 'Acquire::Retries "3";' > /etc/apt/apt.conf.d/80retries

RUN apt-get update && apt-get install -y --no-install-recommends --fix-missing \
    ca-certificates \
    libtbb12 \
    zlib1g \
    libbz2-1.0 \
    liblzma5 \
    libdeflate0 \
    libzstd1 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# 非 root 用户
ARG USERNAME=fqcompressor
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd -g $USER_GID $USERNAME \
    && useradd -m -s /bin/bash -u $USER_UID -g $USER_GID $USERNAME

WORKDIR /app

# 复制构建产物
COPY --from=build /opt/fqcompressor/ /usr/local/

# 数据目录
RUN mkdir -p /app/data /app/output && \
    chown -R $USERNAME:$USERNAME /app

USER $USERNAME

ENV PATH=/usr/local/bin:$PATH
ENV FQCOMPRESSOR_DATA_DIR=/app/data
ENV FQCOMPRESSOR_OUTPUT_DIR=/app/output

HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
    CMD fqc --help || exit 1

CMD ["fqc", "--help"]
