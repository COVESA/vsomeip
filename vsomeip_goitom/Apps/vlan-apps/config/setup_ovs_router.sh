#!/bin/bash
set -e

# Your internal interface (connects to Internal Network in VirtualBox)
INTERNAL_IF="enp0s3"

echo "🛠 Checking if Open vSwitch is installed..."
if ! command -v ovs-vsctl &> /dev/null; then
    echo "🔄 Open vSwitch not found. Installing..."
    sudo apt update
    sudo apt install -y openvswitch-switch
else
    echo "✅ Open vSwitch is already installed."
fi

echo "🌉 Checking if bridge 'br0' exists..."
if sudo ovs-vsctl br-exists br0; then
    echo "✔️  Bridge 'br0' already exists."
else
    echo "➕ Creating bridge 'br0'..."
    sudo ovs-vsctl add-br br0
fi

echo "🔗 Checking if $INTERNAL_IF is attached to br0..."
if sudo ovs-vsctl list-ports br0 | grep -q "^$INTERNAL_IF$"; then
    echo "✔️  $INTERNAL_IF is already part of br0."
else
    echo "➕ Adding $INTERNAL_IF to br0..."
    sudo ovs-vsctl add-port br0 "$INTERNAL_IF"
fi

echo "🧱 Creating VLAN interfaces..."

# VLAN 10
if ip link show vlan10 &>/dev/null; then
    echo "✔️  VLAN interface 'vlan10' already exists."
else
    echo "➕ Creating VLAN 10 (vlan10)..."
    sudo ovs-vsctl add-port br0 vlan10 tag=10 -- set interface vlan10 type=internal
    sudo ip link set dev vlan10 up
    sudo ip addr add 192.168.10.1/24 dev vlan10
fi

# VLAN 20
if ip link show vlan20 &>/dev/null; then
    echo "✔️  VLAN interface 'vlan20' already exists."
else
    echo "➕ Creating VLAN 20 (vlan20)..."
    sudo ovs-vsctl add-port br0 vlan20 tag=20 -- set interface vlan20 type=internal
    sudo ip link set dev vlan20 up
    sudo ip addr add 192.168.20.1/24 dev vlan20
fi

echo "🔄 Enabling IP forwarding..."
if grep -q '^net.ipv4.ip_forward=1' /etc/sysctl.conf; then
    echo "✔️  IP forwarding already enabled."
else
    echo "➕ Enabling IP forwarding..."
    echo 'net.ipv4.ip_forward=1' | sudo tee -a /etc/sysctl.conf
    sudo sysctl -w net.ipv4.ip_forward=1
fi

echo "✅ Router OVS setup complete!"

echo "Verifying network connection:"
sudo ip link
sudo ip addr
sudo ip route

echo "✅ Router setup complete!"

#Optional cleanup
# sudo ovs-vsctl del-br br0
