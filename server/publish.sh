DATE=$(date +'%Y-%m-%d')
docker buildx build --platform linux/amd64,linux/arm64 -t "davweb/dashboard:${DATE}" -t "davweb/dashboard:latest" --push .