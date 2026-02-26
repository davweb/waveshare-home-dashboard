FROM alpine:edge

# Install python
RUN apk add --no-cache python3
# Build a virtual env to build the python dependencies in to
RUN python3 -m venv /app/venv
# Install python dependenices into temporary build image
COPY requirements.txt /app
RUN source /app/venv/bin/activate && pip3 install --no-cache-dir --requirement /app/requirements.txt

WORKDIR /app

# Copy the app files
COPY dashboard dashboard
COPY entrypoint.sh /app

# Make python unbuffered to keep logs as up to date as possible
ENV PYTHONUNBUFFERED=1

# Run our entrypoint
CMD ["sh", "/app/entrypoint.sh"]
