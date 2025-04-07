FROM gcc:13 AS builder

# Install CMake and other dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    libssl-dev \
    zlib1g-dev \
    libgomp1 \
    libjemalloc-dev \
    && rm -rf /var/lib/apt/lists/*

# Set up working directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory
RUN mkdir -p build

# Build the application
WORKDIR /app/build
RUN cmake .. -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DUSE_JEMALLOC=On
RUN cmake --build . -j $(nproc) -t mithril_crawler && test -f crawler/mithril_crawler

# Use a smaller image for the final container
FROM ubuntu:24.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libssl3 \
    zlib1g \
    libgomp1 \
    libjemalloc2 \
    ca-certificates \
    && update-ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create a directory for the application
WORKDIR /app

# Copy the built binary from the builder stage
COPY --from=builder /app/build/crawler/mithril_crawler /app/

# Set the default command
ENTRYPOINT ["/app/mithril_crawler"]
