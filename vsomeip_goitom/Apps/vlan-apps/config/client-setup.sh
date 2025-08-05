#!/bin/bash
set -e

# --- CONFIGURATION ---
NS_NAME="client-ns"           # Name of the namespace (change as needed)
PARENT_IF="enp0s3"             # Main VM interface (change as needed)
VLAN_ID="10"                   # VLAN ID (change as needed)
VLAN_IF="${PARENT_IF}.${VLAN_ID}"
NS_CL="192.168.10.2/24"        # IP for the namespace (change as needed)
NS_SRV="192.168.20.2/24"        # IP for the namespace (change as needed)
GW_IP="192.168.10.1"           # Gateway (Router VM's VLAN IP)
CL_IP="192.168.10.2"
SRV_IP="192.168.20.2"

# --- 1. Create the namespace ---
echo "Creating namespace $NS_NAME..."
sudo ip netns add "$NS_NAME" 2>/dev/null || { echo "Failed to create $NS_NAME"; exit 1; }

# --- 2. Create the VLAN subinterface in the root namespace ---
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
    echo "➕ Creating $VLAN_IF on $PARENT_IF for VLAN $VLAN_ID..."
    sudo ip link add link "$PARENT_IF" name "$VLAN_IF" type vlan id "$VLAN_ID" 2>/dev/null || { echo "Failed to create $VLAN_IF"; exit 1; }
fi

# --- 3. Move the VLAN subinterface into the namespace ---
echo "Moving $VLAN_IF to $NS_NAME..."
sudo ip link set "$VLAN_IF" netns "$NS_NAME" || { echo "Failed to move $VLAN_IF to $NS_NAME"; exit 1; }

# --- 4. Assign IP and bring up inside the namespace ---
echo "Assigning IP $NS_SRV to $VLAN_IF..."
sudo ip netns exec "$NS_NAME" ip addr add "$NS_CL" dev "$VLAN_IF" || { echo "Failed to assign IP to $VLAN_IF"; exit 1; }
echo "Bringing up $VLAN_IF in $NS_NAME..."
sudo ip netns exec "$NS_NAME" ip link set "$VLAN_IF" up || { echo "Failed to bring up $VLAN_IF in $NS_NAME"; exit 1; }
sudo ip netns exec "$NS_NAME" ip link set lo up

# --- 5. Set default route in the namespace ---
echo "Configuring routing in $NS_NAME..."
sudo ip netns exec "$NS_NAME" ip route add default via "$GW_IP" dev "$VLAN_IF" 2>/dev/null

# --- 6. (Optional) Add multicast route in the namespace ---
echo "Configuring multicast in $NS_NAME..."
sudo ip netns exec "$NS_NAME" ip route add 224.244.224.245/32 dev "$VLAN_IF" 2>/dev/null || true

# Configure IPsec
## IPsec state for unicast traffic
echo "Configuring IPsec in $NS_NAME..."
sudo ip netns exec $NS_NAME ip xfrm state add src $CL_IP dst $SRV_IP proto esp spi 0x100 mode tunnel \
  enc aes 0x1234567890abcdef1234567890abcdef auth sha1 0xabcdef1234567890abcdef1234567890abcdef12 2>/dev/null || true
sudo ip netns exec $NS_NAME ip xfrm state add src $SRV_IP dst $CL_IP proto esp spi 0x101 mode tunnel \
  enc aes 0xabcdef1234567890abcdef1234567890 auth sha1 0x1234567890abcdef1234567890abcdef12345678 2>/dev/null || true
## IPsec policy for unicast traffic (no port constraints)
sudo ip netns exec $NS_NAME ip xfrm policy add src $NS_CL dst $NS_SRV dir out \
  tmpl src $CL_IP dst $SRV_IP proto esp mode tunnel 2>/dev/null || true
sudo ip netns exec $NS_NAME ip xfrm policy add src $NS_SRV dst $NS_CL  dir in \
  tmpl src $SRV_IP dst $CL_IP proto esp mode tunnel 2>/dev/null || true

# --- 7. (Optional) Configure firewall in the namespace ---
echo "Configuring firewall in $NS_NAME..."
sudo ip netns exec "$NS_NAME" iptables -A INPUT -p icmp -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A OUTPUT -p icmp -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A INPUT -p igmp -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A OUTPUT -p igmp -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A INPUT -p udp --dport 30490 -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A OUTPUT -p udp --dport 30490 -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A INPUT -p udp --dport 30509 -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A OUTPUT -p udp --sport 30509 -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A INPUT -p udp -d 224.244.224.245 --dport 30490 -j ACCEPT || true
sudo ip netns exec "$NS_NAME" iptables -A OUTPUT -p udp -d 224.244.224.245 --dport 30490 -j ACCEPT || true

# --- 8. Verification ---
echo "Verifying network configuration in $NS_NAME:"
echo "Namespace $NS_NAME interfaces:"
sudo ip netns exec "$NS_NAME" ip link
sudo ip netns exec "$NS_NAME" ip addr
echo "Namespace $NS_NAME routes:"
sudo ip netns exec "$NS_NAME" ip route
sudo ip netns exec "$NS_NAME" ip xfrm state
sudo ip netns exec "$NS_NAME" ip xfrm policy
echo "Pinging gateway $GW_IP from $NS_NAME:"
sudo ip netns exec "$NS_NAME" ping -c 4 "$GW_IP" || true
echo "Pinging service $SRV_IP from $NS_NAME:"
sudo ip netns exec "$NS_NAME" ping -c 4 $SRV_IP || true

#sudo ip netns exec client-ns ping -c 4 192.168.10.1
#sudo ip netns exec client-ns ping -c 4 192.168.20.1
#sudo ip netns exec client-ns ping -c 4 192.168.20.2

echo "✅ Namespace $NS_NAME with VLAN $VLAN_ID setup complete!"
echo "To run your vsomeip app:"
echo "sudo ip netns exec $NS_NAME env LD_LIBRARY_PATH=/path/to/vsomeip build \
    VSOMEIP_CONFIGURATION=/path/to/vsome json configuration VSOMEIP_APPLICATION_NAME=... ./your_app"