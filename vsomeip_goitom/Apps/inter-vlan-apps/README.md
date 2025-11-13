 # Getting Started

 - Configure the VMs by running the configuration under script.
 - Make sure the VMs are able to communicate by running ping command on each host.
 - Check the README under local-apps for further configuration issue.
 - Build the vsomeip library according to https://github.com/COVESA/vsomeip README. It is recommended to build it with signal handling(cmake -DENABLE_SIGNAL_HANDLING=1)
 
 ### Compilation of inter-vlan-apps examples

cd <root directory of vSomeIP-goitom-Lib>/Apps/inter-vlan-apps$:

mkdir build
cd build
cmake .. or (sudo cmake -Dvsomeip3_DIR=/path/to/vsomeip/build ..)
make

### Configure Router VM(R VM)
___________________________
    1. sudo ovs-vsctl del-br br0
    2. ./setup_ovs_router.sh
    3. sudo systemctl restart smcrouted