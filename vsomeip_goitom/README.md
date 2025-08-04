# Getting Started

1. Install VirtualBox
2. Install two Ubuntu Machines on the VirtualBox
3. Network the two Machines:Grok/ChatLLM: Networking Linux VMs on VirtualBox
To enable communication between two Linux virtual machines (VMs) running on VirtualBox 7 on a Windows 11 host, with IP addresses 192.168.10.1 and 192.168.10.4 on the same network (192.168.10.x), you need to configure the VirtualBox network settings and ensure the VMs can communicate. Below are the steps to set up and verify communication:

### 1. **Configure VirtualBox Network Settings**
   - Ensure both VMs are configured to use the same network type in VirtualBox to allow them to communicate with each other and, if needed, the host or external networks.
   - The most straightforward setup for communication between VMs on the same network is to use the **Internal Network** or **Bridged Adapter** mode. For your scenario (192.168.10.x), **Bridged Adapter** or **NAT Network** is likely appropriate, as these allow the VMs to be on the same subnet as your host or a custom network.

   **Steps:**
   1. **Shut down both VMs** if they are running.
   2. In VirtualBox, select the first VM (e.g., 192.168.10.1):
      - Go to **Settings > Network > Adapter 1**.
      - Set **Attached to** to **Bridged Adapter** (if you want the VMs to communicate with the host and external network) or **NAT Network** (for a private network within VirtualBox).
      - If using **Bridged Adapter**, select the host’s network interface (e.g., Wi-Fi or Ethernet) that connects to your local network.
      - If using **NAT Network**, ensure a NAT Network is created:
        - Go to **File > Preferences > Network** in VirtualBox.
        - Click the **+** button to create a new NAT Network (e.g., name it `NatNetwork1`) and set the network to `192.168.10.0/24`.
      - Ensure **Enable Network Adapter** is checked.
   3. Repeat the same steps for the second VM (192.168.10.4), ensuring it’s on the same **Bridged Adapter** or **NAT Network** as the first VM.
   4. Start both VMs.


### 2. **Set Static IP Addresses in Linux VMs**
   Since you’ve specified the IPs as 192.168.10.1 and 192.168.10.4, you likely want static IPs. Configure these in each Linux VM.

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


### 3. **Test Communication Between VMs**
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

### 4. **Optional: Communicate with the Host (Windows 11)**
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

### 5. **Set Up Additional Communication (e.g., SSH, File Sharing)**
   If you want to enable specific types of communication:
   - **SSH**:
     1. Install SSH server on both VMs:
        ```bash
        sudo apt install openssh-server  # Ubuntu/Debian
        sudo yum install openssh-server  # CentOS/RHEL
        ```
     2. Start and enable the SSH service:
        ```bash
        sudo systemctl enable ssh
        sudo systemctl start ssh
        ```
     3. From VM1, connect to VM2:
        ```bash
        ssh user@192.168.10.4
        ```
     4. Ensure the firewall allows SSH (port 22):
        - Ubuntu/Debian: `sudo ufw allow 22`
        - CentOS/RHEL: `sudo firewall-cmd --add-port=22/tcp --permanent; sudo firewall-cmd --reload`
   - **File Sharing (e.g., NFS or Samba)**:
     - For NFS (Linux-to-Linux):
       1. On VM1 (server), install NFS:
          ```bash
          sudo apt install nfs-kernel-server  # Ubuntu/Debian
          sudo yum install nfs-utils  # CentOS/RHEL
          ```
       2. Configure NFS exports in `/etc/exports`:
          ```bash
          /shared-folder 192.168.10.0/24(rw,sync,no_subtree_check)
          ```
       3. Export and start NFS:
          ```bash
          sudo exportfs -a
          sudo systemctl restart nfs-kernel-server
          ```
       4. On VM2 (client), mount the share:
          ```bash
          sudo mount 192.168.10.1:/shared-folder /mnt
          ```
     - For Samba (if sharing with Windows host):
       1. Install Samba on the VM:
          ```bash
          sudo apt install samba  # Ubuntu/Debian
          sudo yum install samba  # CentOS/RHEL
          ```
       2. Configure Samba and add shares in `/etc/samba/smb.conf`.
       3. Access from Windows using `\\192.168.10.1\share`.

### 6. **Troubleshooting Tips**
   - **No ping response**: Verify VirtualBox network mode, check firewall settings, and ensure IPs are correctly set.
   - **Network not detected**: Restart the VMs or VirtualBox. Check the VirtualBox network adapter status.
   - **Host communication issues**: Ensure the Windows firewall allows traffic from the 192.168.10.x subnet.
   - **NAT Network issues**: Confirm the NAT Network is configured in VirtualBox preferences with the correct subnet (192.168.10.0/24).

### Example Setup Summary
- VM1: 192.168.10.1, Bridged Adapter or NAT Network, static IP configured.
- VM2: 192.168.10.4, same network mode as VM1, static IP configured.
- Test with `ping`, then set up SSH or file sharing as needed.
- Host (Windows 11): Accessible if using Bridged Adapter; check its IP and firewall.

If you need specific configurations (e.g., for a particular Linux distribution, SSH setup, or file sharing), let me know, and I can provide more tailored instructions!
4. Clone a vsomeip Project : https://github.com/getgou/vsomeip#
5. Execute the hello World examples following the readme: https://github.com/COVESA/vsomeip/blob/master/examples/hello_world/readme
 Error: 1 Configuration module could not be loaded !
Solution: https://github.com/COVESA/vsomeip/issues/480
export LD_LIBRARY_PATH=/usr/local/lib in the ~/.bashrc file
6. To use the example applications you need two devices on the same network. The network addresses within the configuration files need to be adapted to match the devices addresses.
7. Compile the example source codes
    mkdir build
    cd build
    cmake ..
    make examples
8. Add the multicast routes:
    sudo route add 224.244.224.245 dev enp0s3
    sudo route add 224.225.226.233 dev enp0s3
9. [warning] local_client_endpoint::connect: Couldn't connect to: /tmp/vsomeip-0 (No such file or directory / 2) #43
    Update request-sample.cpp and subscribe-sample.cpp with app_(vsomeip::runtime::get()->create_application("service-sample")). i.e. app_(vsomeip::runtime::get()->create_application(<value of the routing>))

## Working with the examples

   ### Compilation of examples

For compilation of the examples call:

```bash
mkdir build
cd build
cmake ..
make examples
```
1. Starting the sample applications
    To start the request/response-example from the build-directory do:

    HOST1: env VSOMEIP_CONFIGURATION=../../vsomeip_goitom/config/vsomeip.json VSOMEIP_APPLICATION_NAME=client-sample ./request-sample
    HOST2: env VSOMEIP_CONFIGURATION=../../vsomeip_goitom/config/vsomeip.json VSOMEIP_APPLICATION_NAME=service-sample ./response-sample

    To start the subscribe/notify-example from the build-directory do:

    HOST1: env VSOMEIP_CONFIGURATION=../../vsomeip_goitom/config/vsomeip.json VSOMEIP_APPLICATION_NAME=client-sample ./subscribe-sample
    HOST2: env VSOMEIP_CONFIGURATION=../../vsomeip_goitom/config/vsomeip.json VSOMEIP_APPLICATION_NAME=service-sample ./notify-sample


# References
1. https://github.com/COVESA/vsomeip/wiki/vsomeip-in-10-minutes
2. https://github.com/COVESA/vsomeip/tree/master/examples/hello_world
3. https://gist.github.com/qin-yu/bc26a2d280ee2e93b2d7860a1bfbd0c5
4. https://grok.com/chat/c98a90b8-dfc8-473a-a7a5-dd9c84ecfd5d
5. https://github.com/COVESA/vsomeip/issues/43
