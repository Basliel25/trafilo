FROM debian:12-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        bash \
        netcat-openbsd \
        coreutils \
        gawk \
        grep \
    && rm -rf /var/lib/apt/lists/*

COPY dummy_dameon.sh /dummy_dameon.sh
RUN chmod +x /dummy_dameon.sh

ENTRYPOINT ["/dummy_dameon.sh"]
