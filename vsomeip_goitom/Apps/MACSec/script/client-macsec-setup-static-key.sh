#!/bin/bash
PARENT_IF="enp0s3"
MAC_IF="macsec0"
PEER_MAC="08:00:27:ad:c3:bd" # Retrieve it with ip link
MY_IP="192.168.10.4/24"
MY_PEER_IP="192.168.10.1"
KEY="0123456789abcdef0123456789abcdef" # Static key

echo "🔌 Loading modprobe toolset..."
sudo modprobe macsec

sudo ip link add link $PARENT_IF $MAC_IF type macsec encrypt on
sudo ip macsec add $MAC_IF tx sa 0 pn 1 on key 01 $KEY
sudo ip macsec add $MAC_IF rx address $PEER_MAC port 1
sudo ip macsec add $MAC_IF rx address $PEER_MAC port 1 sa 0 pn 1 on key 01 $KEY
sudo ip link set $MAC_IF up
sudo ip addr add $MY_IP dev $MAC_IF

sudo ip route add 224.244.224.245/32 dev $MAC_IF
sudo ip route add 192.168.10.4/32 dev $MAC_IF

sudo echo "Check connection status"
sudo ping -c 4 $MY_PEER_IP
sudo echo "Check MACsec status"
sudo ip -d link show $MAC_IF
sudo echo "MACsec configuration setup complete"
# delete the interface
#sudo ip link del macsec0