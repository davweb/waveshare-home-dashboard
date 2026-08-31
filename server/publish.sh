#!/bin/sh
DATE=$(date +'%Y-%m-%d')
docker buildx build --platform linux/amd64,linux/arm64 -t "ghcr.io/davweb/dashboard:${DATE}" -t "ghcr.io/davweb/dashboard:latest" --push .
