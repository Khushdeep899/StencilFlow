# Two-stage build for StencilFlow.
#
# The builder stage has cmake, gcc, OpenMPI development headers,
# and the OpenMP runtime. It compiles the project and runs the
# unit tests so the image is only published if tests pass.
#
# The runtime stage carries only what mpirun needs at runtime.
# That keeps the published image small and surface area minimal.

# ------- builder -------
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        libopenmpi-dev \
        openmpi-bin \
        libomp-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

# Run unit tests in the builder so a failing test breaks the build.
RUN ctest --test-dir build --output-on-failure

# ------- runtime -------
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        openmpi-bin \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash stencil

COPY --from=builder /src/build/stencilflow /usr/local/bin/stencilflow

USER stencil
WORKDIR /home/stencil

# Default: 4 ranks, 256x256, 500 steps, frame every 100. Override
# the args to docker run for different problem sizes.
CMD ["mpirun", "-n", "4", "/usr/local/bin/stencilflow", "256", "256", "500", "100"]
