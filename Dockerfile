FROM gcc:14-bookworm AS build
WORKDIR /build
RUN apt-get update && apt-get install -y --no-install-recommends libssl-dev && rm -rf /var/lib/apt/lists/*
COPY vendor vendor
COPY src src
RUN gcc -O2 -c vendor/sqlite3.c -o sqlite.o && g++ -std=c++20 -O2 -pthread -Ivendor src/main.cpp sqlite.o -ldl -lcrypto -static-libstdc++ -static-libgcc -o hotel-kingpin
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libssl3 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /build/hotel-kingpin .
COPY public public
RUN mkdir data && chown 65534:65534 data
USER 65534:65534
EXPOSE 8080
CMD ["./hotel-kingpin"]
