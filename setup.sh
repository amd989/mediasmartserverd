#!/bin/bash
set -e
echo "Adding mediasmartserverd APT repository..."
curl -fsSL https://amd989.github.io/mediasmartserverd/gpg.key | gpg --dearmor -o /usr/share/keyrings/mediasmartserverd.gpg
echo "deb [signed-by=/usr/share/keyrings/mediasmartserverd.gpg] https://amd989.github.io/mediasmartserverd stable main" > /etc/apt/sources.list.d/mediasmartserverd.list
apt-get update
echo "Done! You can now run: sudo apt install mediasmartserver"
