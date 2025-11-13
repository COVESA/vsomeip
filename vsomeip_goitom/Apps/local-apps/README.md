# Getting Started

## 1. **Configure VirtualBox Network Settings**
   - If the setup is between two hosts, ensure both VMs are configured to use the same network type in VirtualBox to allow them to communicate with each other and, if needed, the host or external networks.
   - The most straightforward setup for communication between VMs on the same network is to use the **Internal Network** mode. if internet connection in the VMs is needed, **Bridged Adapter** mode is the recommended one. 

   **Steps:**
   1. **Shut down both VMs** if they are running.
   2. In VirtualBox, select the first VM (e.g., 192.168.10.1):
      - Go to **Settings > Network > Adapter 1**.
      - Set **Attached to** to **Bridged Adapter** (if you want the VMs to communicate with the host and external network) or **Internal Network** (for a private network within VirtualBox).
      - If using **Bridged Adapter**, select the host’s network interface (e.g., Wi-Fi or Ethernet) that connects to your local network.
      - If using **Internal Network**, ensure an intnet Network is created:
        - Go to **File > Preferences > Network** in VirtualBox.
        - Click the **+** button to create a new **Internal Network**  Network (e.g., name it `intnet`) and set the network to `192.168.10.0/24`.
      - Ensure **Enable Network Adapter** is checked.
   3. Repeat the same steps for the second VM (192.168.10.4), ensuring it’s on the same **Bridged Adapter** or **Internal Network**  as the first VM.
   4. Start both VMs.


## 2. **Set Static IP Addresses in Linux VMs**

   **For Ubuntu/Debian-based VMs:**
   1. Edit the network configuration file. For modern Ubuntu versions using Netplan:
      - Open a terminal and edit the Netplan configuration file (e.g., `/etc/netplan/01-netcfg.yaml`):
        ```bash
        sudo nano /etc/netplan/01-netcfg.yaml
        ```
      - For VM1 (192.168.10.1), add or modify:
        ```yaml
        network:
          version: 2
          ethernets:
            enp0s3:  # Replace with your network interface name (check with `ip a`)
              addresses:
                - 192.168.10.1/24
              #gateway4: 192.168.10.1  # Optional, adjust if a gateway is needed
              #nameservers:
                #addresses: [8.8.8.8, 8.8.4.4]
        ```
      - For VM2 (192.168.10.4), use the same configuration but with `addresses: - 192.168.10.4/24`.
   2. Apply the changes:
      ```bash
      sudo netplan apply
      ```
   3. Verify the IP address:
      ```bash
      ip a

## 3. **Test Communication Between VMs**
   Once the network is configured and IPs are set:
   1. From VM1 (192.168.10.1), ping VM2:
      ```bash
      ping 192.168.10.4
      ```
   2. From VM2 (192.168.10.4), ping VM1:
      ```bash
      ping 192.168.10.1
      ```
   - If pings succeed, communication is working.
   - If pings fail, check the following:
     - Ensure the VirtualBox network settings match for both VMs (same Bridged Adapter or NAT Network).
     - Verify the firewall settings on both VMs:
       - For Ubuntu/Debian:
         ```bash
         sudo ufw allow proto tcp from 192.168.10.0/24
         sudo ufw status
         ```

     - Ensure the VMs are on the same subnet (e.g., 192.168.10.0/24).
     - Check VirtualBox network adapter status in the VM settings.

## 4. **Optional: Communicate with the Host (Windows 11)**
   If you want the VMs to communicate with the Windows 11 host:
   - Use **Bridged Adapter** mode so the VMs are on the same network as the host.
   - Find the host’s IP address on the 192.168.10.x network:
     - On Windows 11, run in Command Prompt or PowerShell:
       ```powershell
       ipconfig
       ```
       Look for the IPv4 address of the relevant network adapter (e.g., 192.168.10.x).
   - From either VM, ping the host’s IP:
      ```bash
      ping <host-ip>
      ```
   - If the host doesn’t respond, check the Windows Defender Firewall:
     - Open **Windows Defender Firewall > Advanced Settings > Inbound Rules**.
     - Create a new rule to allow ICMPv4 (ping) or specific protocols/ports for your needs.

## 5. **Troubleshooting Tips**
   - **No ping response**: Verify VirtualBox network mode, check firewall settings, and ensure IPs are correctly set.
   - **Network not detected**: Restart the VMs or VirtualBox. Check the VirtualBox network adapter status.
   - **Host communication issues**: Ensure the Windows firewall allows traffic from the 192.168.10.x subnet.
   - **Internal Network**: Confirm the  Network is configured in VirtualBox preferences with the correct subnet (192.168.10.0/24).

## 6. Working with the examples
1. Clone a vsomeip Project : 
    https://github.com/COVESA/vsomeip/
2. Execute the hello World examples following the readme: 
    https://github.com/COVESA/vsomeip/blob/master/examples/hello_world/readme
 
 If you face Error: 1 Configuration module could not be loaded !, 
    Solution: https://github.com/COVESA/vsomeip/issues/480
    export LD_LIBRARY_PATH=/usr/local/lib in the ~/.bashrc file

3. To use the example applications you need two devices on the same network. The network addresses within the configuration files need to be adapted to match the devices addresses.
4. Compile the example source codes
```bash
    mkdir build
    cd build
    cmake ..
    make examples
```
5. Add the multicast routes:
    sudo ip route add 224.244.224.245 dev enp0s3

6. [warning] local_client_endpoint::connect: Couldn't connect to: /tmp/vsomeip-0 (No    such file or directory / 2) #43
    Check if VSOMEIP_APPLICATION_NAME and the routing matches.

### Compilation of local-apps examples

cd <root directory of vSomeIP-goitom-Lib>/Apps/local-apps$:

mkdir build
cd build
cmake .. or (sudo cmake -Dvsomeip3_DIR=/path/to/vsomeip/build ..)
make

# References
1. https://github.com/COVESA/vsomeip/wiki/vsomeip-in-10-minutes
2. https://github.com/COVESA/vsomeip/tree/master/examples/hello_world
3. https://gist.github.com/qin-yu/bc26a2d280ee2e93b2d7860a1bfbd0c5
4. https://grok.com/chat/c98a90b8-dfc8-473a-a7a5-dd9c84ecfd5d
5. https://github.com/COVESA/vsomeip/issues/43