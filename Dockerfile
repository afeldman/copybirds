FROM debian:bookworm-slim
COPY cb /usr/local/bin/cb
ENTRYPOINT ["cb"]
