
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN python3 tools/generate_building.py && make -C backend server

ENV PORT=10000
EXPOSE 10000

CMD ["sh", "-c", "./backend/server . ${PORT:-10000}"]
