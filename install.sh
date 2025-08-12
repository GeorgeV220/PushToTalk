#!/bin/bash

set -e

# Prevent running as root
if [ "$EUID" -eq 0 ]; then
    echo "Please do NOT run this script as root or with sudo."
    echo "Run it as your normal user. The script will ask for sudo when needed."
    exit 1
fi

PREFIX=/usr/local
BUILD_DIR=build
BIN_DIR=$PREFIX/bin
SYSTEMD_DIR=/etc/systemd/system
USER_SYSTEMD_DIR=$HOME/.config/systemd/user

# Build
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

# Install binaries (sudo for server binary only)
sudo install -Dm755 "$BUILD_DIR/ptt-server" "$BIN_DIR/ptt-server"
install -Dm755 "$BUILD_DIR/ptt-client" "$BIN_DIR/ptt-client"

# Create 'ptt' group if it doesn't exist
if ! getent group ptt > /dev/null; then
    echo "Group 'ptt' does not exist. Creating it now..."
    sudo groupadd ptt
    echo "Please add your user to the 'ptt' group to allow running the services without permission issues:"
    echo "  sudo usermod -aG ptt $USER"
    echo "After adding yourself to the group, you may need to log out and back in."
fi

# Setup systemd

# Replace ExecStart path in ptt-server.service and install it (needs sudo)
sudo sed "s|ExecStart=/usr/bin/ptt-server|ExecStart=$BIN_DIR/ptt-server|" ptt-server.service > /tmp/ptt-server.service
sudo install -Dm644 /tmp/ptt-server.service "$SYSTEMD_DIR/ptt-server.service"
rm /tmp/ptt-server.service

mkdir -p "$USER_SYSTEMD_DIR"

# Replace ExecStart path in ptt-client.service and install it (user-level, no sudo)
sed -i "s|ExecStart=/usr/bin/ptt-client|ExecStart=$BIN_DIR/ptt-client|" ptt-client.service
install -Dm644 ptt-client.service "$USER_SYSTEMD_DIR/ptt-client.service"

echo "Installed! Now run:"
echo "  sudo systemctl enable --now ptt-server"
echo "  systemctl --user enable --now ptt-client"
