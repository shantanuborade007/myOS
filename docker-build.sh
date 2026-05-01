#!/bin/bash

echo "Building Docker image..."
docker build -t myos-builder .

echo "Running build inside Docker container..."
# Mounts the current directory into /os inside the container
docker run --rm -v $(pwd):/os myos-builder

echo "Build complete! You can now run the OS using: make run"
