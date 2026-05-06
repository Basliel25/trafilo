FROM debian:12-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        bash \
        netcat-openbsd \
        coreutils \
        gawk \
        grep \
    && rm -rf /var/lib/apt/lists/*

COPY daemon.sh /daemon.sh
RUN chmod +x /daemon.sh

ENTRYPOINT ["/daemon.sh"]
