FROM alpine:edge

# Install python and uv
RUN apk add --no-cache python3 uv

WORKDIR /app

# Install dependencies from lockfile
COPY pyproject.toml uv.lock /app/
RUN uv sync --frozen --no-dev

# Copy the app files
COPY dashboard dashboard
COPY entrypoint.sh /app

# Make python unbuffered to keep logs as up to date as possible
ENV PYTHONUNBUFFERED=1

# Run our entrypoint
CMD ["sh", "/app/entrypoint.sh"]
