FROM debian:11-slim

# Install dependencies
RUN apt-get update -y
RUN apt-get install -y build-essential cmake pkg-config libssl-dev libpcre2-dev

# Copy source code and build openelp
COPY . /openelp
WORKDIR /openelp
RUN mkdir build
WORKDIR /openelp/build
RUN cmake ..
RUN make

# Switch to a new stage
FROM debian:11-slim

# Copy the binary from the previous stage
COPY --from=0 /openelp/build/src/openelpd /bin/openelpd
COPY --from=0 /openelp/build/src/libopenelp.so.0 /lib/libopenelp.so.0

# Run the binary
ENTRYPOINT ["/bin/openelpd", "-F"]