 # Build the examples
___________________________
cd <root directory of vSomeIP-goitom-Lib>/Apps/vlan-apps$:

mkdir build
cd build
cmake ..
make

# Configure Router VM(R VM)
___________________________
    1. sudo ovs-vsctl del-br br0
    2. ./setup_ovs_router.sh
    3. sudo systemctl restart smcrouted