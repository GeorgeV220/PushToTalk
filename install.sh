#!/bin/bash

set -e

PREFIX=/usr/local
BUILD_DIR=build
BIN_DIR=$PREFIX/bin
SYSTEMD_DIR=/etc/systemd/system
USER_SYSTEMD_DIR=$HOME/.config/systemd/user

# Build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release
cmake --build $BUILD_DIR

# Install binaries
sudo install -Dm755 $BUILD_DIR/ptt-server $BIN_DIR/ptt-server
install -Dm755 $BUILD_DIR/ptt-client $BIN_DIR/ptt-client

# Setup systemd
sudo install -Dm644 ptt-server.service.in /tmp/ptt-server.service
DEFAULT_USER=$(awk -F: '$3 == 1000 {print $1}' /etc/passwd)
sudo sed -i "s/__USERNAME__/$DEFAULT_USER/" /tmp/ptt-server.service
sudo install -Dm644 /tmp/ptt-server.service $SYSTEMD_DIR/ptt-server.service

mkdir -p $USER_SYSTEMD_DIR
install -Dm644 ptt-client.service $USER_SYSTEMD_DIR/ptt-client.service

echo "Installed! Now run:"
echo "  sudo systemctl enable --now ptt-server"
echo "  systemctl --user enable --now ptt-client"
