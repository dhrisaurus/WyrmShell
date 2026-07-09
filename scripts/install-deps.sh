#!/usr/bin/env bash
# Installs build + runtime dependencies for wyrmshell across common distros.
# Detects your package manager and runs the right install command.
# Run with: bash scripts/install-deps.sh

set -e

echo "wyrmshell: detecting package manager..."

if command -v zypper >/dev/null 2>&1; then
    echo "-> openSUSE (zypper) detected"
    sudo zypper install -y gcc make gtk3-devel vte-devel fastfetch python3

elif command -v dnf >/dev/null 2>&1; then
    echo "-> Fedora/RHEL-family (dnf) detected"
    sudo dnf install -y gcc make gtk3-devel vte291-devel fastfetch python3

elif command -v apt-get >/dev/null 2>&1; then
    echo "-> Debian/Ubuntu-family (apt) detected"
    sudo apt-get update
    sudo apt-get install -y gcc make libgtk-3-dev libvte-2.91-dev python3
    if ! command -v fastfetch >/dev/null 2>&1; then
        echo ""
        echo "NOTE: fastfetch isn't in older Debian/Ubuntu repos."
        echo "If 'apt-get install -y fastfetch' above failed, get it from:"
        echo "  https://github.com/fastfetch-cli/fastfetch/releases"
        echo "  (download the .deb for your architecture and: sudo dpkg -i <file>.deb)"
    fi
    sudo apt-get install -y fastfetch || true

elif command -v pacman >/dev/null 2>&1; then
    echo "-> Arch-family (pacman) detected"
    sudo pacman -S --needed --noconfirm gcc make gtk3 vte3 fastfetch python

elif command -v apk >/dev/null 2>&1; then
    echo "-> Alpine (apk) detected"
    sudo apk add gcc make musl-dev gtk+3.0-dev vte3-dev fastfetch python3

else
    echo "Couldn't detect a known package manager (zypper/dnf/apt/pacman/apk)."
    echo "Install these manually for your distro:"
    echo "  - a C compiler (gcc or clang) + make"
    echo "  - GTK3 development headers (pkg-config name: gtk+-3.0)"
    echo "  - VTE development headers (pkg-config name: vte-2.91)"
    echo "  - fastfetch"
    echo "  - python3"
    exit 1
fi

echo ""
echo "Done. Verify with:"
echo "  pkg-config --exists gtk+-3.0 vte-2.91 && echo OK || echo MISSING"
