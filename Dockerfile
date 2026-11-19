# Stage 1: Build the C web server binary
FROM gcc:13-bookworm AS builder
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN make web

# Stage 2: Minimal runtime image
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-0 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/web ./web
COPY --from=builder /app/public ./public
COPY --from=builder /app/scenarios ./scenarios

EXPOSE 8080
CMD ["./web"]