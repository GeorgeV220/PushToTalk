FROM mcr.microsoft.com/devcontainers/base:ubuntu

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential packages and dependencies (without cmake)
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    wget \
    gpg \
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

# Install latest CMake (3.29.2) from Kitware
RUN wget https://github.com/Kitware/CMake/releases/download/v4.0.2/cmake-v4.0.2-linux-x86_64.sh && \
    chmod +x cmake-v4.0.2-linux-x86_64.sh && \
    ./cmake-v4.0.2-linux-x86_64.sh --skip-license --prefix=/usr/local && \
    rm cmake-v4.0.2-linux-x86_64.sh

# Create non-root user vscode (Devcontainer default)
USER vscode
WORKDIR /home/vscode
