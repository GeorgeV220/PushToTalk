FROM mcr.microsoft.com/devcontainers/base:ubuntu

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libpulse-dev \
    libopenal-dev \
    libmpg123-dev \
    libgtk-3-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    x11-apps \
    && rm -rf /var/lib/apt/lists/*

USER vscode
WORKDIR /home/vscode
