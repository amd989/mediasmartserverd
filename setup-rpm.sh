#!/bin/bash
set -e
echo "Adding mediasmartserverd YUM/DNF repository..."
rpm --import https://amd989.github.io/mediasmartserverd/gpg.key
cat > /etc/yum.repos.d/mediasmartserverd.repo <<REPOEOF
[mediasmartserverd]
name=mediasmartserverd
baseurl=https://amd989.github.io/mediasmartserverd/rpm/
enabled=1
gpgcheck=1
gpgkey=https://amd989.github.io/mediasmartserverd/gpg.key
REPOEOF
echo "Done! You can now run: sudo dnf install mediasmartserver"
