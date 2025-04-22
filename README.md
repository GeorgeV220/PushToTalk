# PushToTalk 🎙️⌨️🖱️

**Push-to-Talk voice communication tool** — control your mic mute/unmute with any keyboard or mouse button. Built with GTK, PulseAudio (PipeWire-compatible), and OpenAL.  
No hotkey listeners, just raw device-level control.

---

## ✨ Features

✅ Support **any input device** (keyboard, mouse, etc.)  
✅ Block your chosen key from other apps while active  
✅ Toggle **mute/unmute** on the default microphone  
✅ **PipeWire / PulseAudio** compatible  
✅ Fully configurable with a GUI: `ptt-client --gui`  
✅ Detect devices with: `sudo ptt-server --detect`  
✅ Config stored in: `~/.config/ptt.properties`  
✅ Optional systemd services for background usage  
✅ Settings applied on service restart

---

## 🧪 Quick Start (Testing)

```bash
git clone https://github.com/GeorgeV220/PushToTalk.git
cd PushToTalk
mkdir build && cd build
cmake ..
make
./ptt-server --detect       # Run as root to see input devices
./ptt-client --gui          # Configure everything via GUI
```

---

## 📦 Arch Linux Install (via PKGBUILD)

To build and install manually on Arch:

```bash
git clone https://github.com/GeorgeV220/PushToTalk.git
cd PushToTalk
mkdir ptt-build
cp PKGBUILD ptt-client.service ptt-server.service.in pushtotalk.install ptt-build
cd ptt-build
makepkg -si
```

This will:
- Build both `ptt-client` and `ptt-server`
- Install them to `/usr/bin/`
- Install systemd unit files for root (server) and user (client)

---

## ⚙️ Systemd Setup (Optional)

### Server (root):
```bash
sudo systemctl enable --now ptt-server.service
```

### Client (user):
```bash
systemctl --user enable --now ptt-client.service
```

**Note:** Restart both services after changing settings:
```bash
sudo systemctl restart ptt-server
systemctl --user restart ptt-client
```

---

## 🔍 Detecting Input Devices

Run this to get your device IDs (requires root):

```bash
sudo ptt-server --detect
```

Copy the device ID into the GUI, or edit:

```
~/.config/ptt.properties
```

Restart services after changes!

---

## 🛠️ Troubleshooting

- Can't mute/unmute? Check PipeWire/PulseAudio is running
- Device isn't detected? Try running `sudo ptt-server --detect` again
- Nothing happens when pressing the key? Ensure you have the correct device id and ev code

---

## 📄 License

MIT — feel free to fork, share, improve!

---

## 👤 Author

Made with love by [GeorgeV22](https://github.com/GeorgeV220)