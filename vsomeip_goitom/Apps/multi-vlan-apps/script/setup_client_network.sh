#!/usr/bin/env bash
set -euo pipefail

# --- Configuration ---
IFACE_BASE="enp0s3"
PHYSICAL_IP="192.168.160.1/24"  # Physical interface IP for routing manager
VLAN1_ID=170
VLAN2_ID=180
VLAN1_IP="192.168.170.1/24"
VLAN2_IP="192.168.180.1/24"
MCAST_ADDR_BASE="224.244.224.244/32"
MCAST_ADDR_170="224.244.224.245/32"
MCAST_ADDR_180="224.244.224.246/32"
# ---

echo "[+] Configuring provider network interfaces"

sudo -v

# Configure physical interface with IP (if not already configured)
echo "[*] Configuring physical interface ${IFACE_BASE}"
if ! ip addr show ${IFACE_BASE} | grep -q "${PHYSICAL_IP%/*}"; then
  sudo ip addr add ${PHYSICAL_IP} dev ${IFACE_BASE} 2>/dev/null || echo "[*] IP already exists on ${IFACE_BASE}"
fi
sudo ip link set ${IFACE_BASE} up

# VLAN 170 setup
if ! ip link show ${IFACE_BASE}.${VLAN1_ID} &>/dev/null; then
  echo "[*] Creating ${IFACE_BASE}.${VLAN1_ID}"
  sudo ip link add link ${IFACE_BASE} name ${IFACE_BASE}.${VLAN1_ID} type vlan id ${VLAN1_ID}
fi
sudo ip addr flush dev ${IFACE_BASE}.${VLAN1_ID} || true
sudo ip addr add ${VLAN1_IP} dev ${IFACE_BASE}.${VLAN1_ID}
sudo ip link set ${IFACE_BASE}.${VLAN1_ID} up

# VLAN 180 setup
if ! ip link show ${IFACE_BASE}.${VLAN2_ID} &>/dev/null; then
  echo "[*] Creating ${IFACE_BASE}.${VLAN2_ID}"
  sudo ip link add link ${IFACE_BASE} name ${IFACE_BASE}.${VLAN2_ID} type vlan id ${VLAN2_ID}
fi
sudo ip addr flush dev ${IFACE_BASE}.${VLAN2_ID} || true
sudo ip addr add ${VLAN2_IP} dev ${IFACE_BASE}.${VLAN2_ID}
sudo ip link set ${IFACE_BASE}.${VLAN2_ID} up

# Add multicast routes
echo "[*] Adding multicast routes..."
sudo ip route add ${MCAST_ADDR_BASE} dev ${IFACE_BASE} 2>/dev/null || echo "[*] Multicast route for ${IFACE_BASE} already exists"
sudo ip route add ${MCAST_ADDR_170} dev ${IFACE_BASE}.${VLAN1_ID} 2>/dev/null || echo "[*] Multicast route for VLAN ${VLAN1_ID} already exists"
sudo ip route add ${MCAST_ADDR_180} dev ${IFACE_BASE}.${VLAN2_ID} 2>/dev/null || echo "[*] Multicast route for VLAN ${VLAN2_ID} already exists"

echo "[+] Provider network ready!"
echo ""

echo "Physical interface:"
ip -br addr show ${IFACE_BASE}
echo ""
echo "VLAN interfaces:"
ip -br addr show ${IFACE_BASE}.${VLAN1_ID}
ip -br addr show ${IFACE_BASE}.${VLAN2_ID}
echo ""
echo "Multicast routes:"
ip route show ${MCAST_ADDR_BASE}
ip route show ${MCAST_ADDR_170}
ip route show ${MCAST_ADDR_180}
