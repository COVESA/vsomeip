#!/bin/bash
set -e

# Variables
PARENT_IF="enp0s3"
MY_IP="192.168.10.1/24"
MY_PEER_IP="192.168.10.4"
MAC_IF="macsec0"
CKN="00112233445566778899aabbccddeeff"
CAK="0123456789abcdef0123456789abcdef"
CONF="/etc/wpa_supplicant-macsec.conf"

# Load macsec kernel module
sudo modprobe macsec

# Create wpa_supplicant config
# macsec_psk=1 =>old versions only
sudo bash -c "cat > $CONF" <<EOF
ctrl_interface=/var/run/wpa_supplicant
ap_scan=0

network={
    key_mgmt=NONE
    macsec_policy=1
    macsec_integ_only=0
    macsec_replay_protect=1
    macsec_replay_window=0
    macsec_port=1    
    mka_ckn=$CKN
    mka_cak=$CAK
}
EOF

# Kill any previous wpa_supplicant
sudo pkill wpa_supplicant || true

# Start wpa_supplicant for MACsec
sudo wpa_supplicant -i $PARENT_IF -D macsec_linux -c $CONF -B

# Wait for macsec0 to appear
for i in {1..10}; do
    if ip link show $MAC_IF &>/dev/null; then
        break
    fi
    sleep 1
done

# Assign IP and bring up (replace makes re-run safe)
sudo ip addr replace $MY_IP dev $MAC_IF
sudo ip link set $MAC_IF up

# Add the route
sudo ip route add 224.244.224.245/32 dev $MAC_IF
sudo ip route add 192.168.10.1/32 dev $MAC_IF

# Show status
echo "Check connection status"
ping -c 4 $MY_PEER_IP || true
echo "Check MACsec status"
ip -d link show $MAC_IF || echo "$MAC_IF not found"
ip macsec show || true
echo "MACsec configuration setup complete"