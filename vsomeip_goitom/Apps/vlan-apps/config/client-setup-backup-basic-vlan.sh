#!/bin/bash
set -e

# Usage: sudo ./setup_vlan_client.sh enp0s3 10 192.168.10.2/24

IFACE="enp0s3"
VLAN_ID="10"
IPADDR="192.168.10.2/24"
VLAN_IF="${IFACE}.${VLAN_ID}"

# if [ -z "$IFACE" ] || [ -z "$VLAN_ID" ] || [ -z "$IPADDR" ]; then
#     echo "❗ Usage: sudo $0 <interface> <vlan_id> <ip_address/CIDR>"
#     exit 1
# fi

# Verify dependencies
for cmd in ip smcroute iptables; do
  if ! command -v $cmd >/dev/null 2>&1; then
    echo "$cmd not found, installing..."
    sudo apt update
    sudo apt install -y iproute2 smcroute iptables
  fi
done

echo "🔌 Checking for VLAN toolset..."
if ! dpkg -s vlan &>/dev/null; then
    echo "🔄 Installing vlan package..."
    sudo apt update
    sudo apt install -y vlan
else
    echo "✅ VLAN package already installed."
fi

echo "🧠 Ensuring 8021q kernel module is loaded..."
if lsmod | grep -q 8021q; then
    echo "✅ 8021q module already loaded."
else
    echo "➕ Loading 8021q module..."
    sudo modprobe 8021q
fi

echo "🔧 Checking for VLAN interface $VLAN_IF..."
if ip link show "$VLAN_IF" &>/dev/null; then
    echo "✔️  $VLAN_IF already exists."
else
    echo "➕ Creating $VLAN_IF on $IFACE for VLAN $VLAN_ID..."
    sudo ip link add link "$IFACE" name "$VLAN_IF" type vlan id "$VLAN_ID"
    sudo ip addr add "$IPADDR" dev "$VLAN_IF"
    sudo ip link set "$VLAN_IF" up
    echo "✅ $VLAN_IF is up with IP $IPADDR"
fi

# Configure routing
echo "Configuring routing ..."
sudo ip route add default via 192.168.10.1 dev enp0s3.10 2>/dev/null || true

# Configure firewall
echo "Configuring firewall..."
sudo  iptables -A INPUT -p icmp -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p icmp -j ACCEPT 2>/dev/null || true
# Especial addition
sudo  iptables -A INPUT -p igmp -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p igmp -j ACCEPT 2>/dev/null || true
# Especial addition end
sudo  iptables -A INPUT -p udp --dport 30490 -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p udp --dport 30490 -j ACCEPT 2>/dev/null || true
sudo  iptables -A INPUT -p udp --dport 30509 -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p udp --sport 30509 -j ACCEPT 2>/dev/null || true
sudo  iptables -A INPUT -p udp -d 224.244.224.245 --dport 30490 -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p udp -d 224.244.224.245 --dport 30490 -j ACCEPT 2>/dev/null || true
sudo  iptables -A INPUT -p udp -d 224.225.226.234 --dport 32344 -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p udp -d 224.225.226.234 --dport 32344 -j ACCEPT 2>/dev/null || true
sudo  iptables -A INPUT -p esp -j ACCEPT 2>/dev/null || true
sudo  iptables -A OUTPUT -p esp -j ACCEPT 2>/dev/null || true

# Configure multicast
echo "Configuring multicast in client-ns..."
sudo  ip route add 224.244.224.245/32 dev enp0s3.10 2>/dev/null || true
sudo  ip route add 224.225.226.234/32 dev enp0s3.10 2>/dev/null || true

# Verify
echo "Verifying network connection:"
sudo ip link
sudo ip addr
sudo ip route
echo "Ping the router:"
sudo ping -c 4 192.168.10.1
echo "Ping the service:"
sudo ping -c 4 192.168.20.2

echo "✅ Client setup complete!"