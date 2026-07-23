#!/bin/bash -eu
# Runs a test command in an isolated environment using bubblewrap (bwrap).
# This provides PID and network namespace isolation so tests can run in
# parallel without interfering with each other.
#
# Network isolation with slave bridging:
#   Each sandbox gets a private virtual L2 segment to its allocated slave(s)
#   via VXLAN tunnel(s). This ensures:
#   - The fixed master IP (TEST_IP_MASTER) is reused identically in every
#     sandbox without conflicts (each namespace is fully isolated).
#   - Multicast between sandbox and slaves works normally on the private segment.
#   - Multicast does NOT leak to the host, other sandboxes, or other slaves.
#
#   Architecture (eth0 is never modified):
#     [master namespace]
#       eth0 ─── native underlay connectivity to slaves (Docker macvlan)
#       vx_${VNI}_$$ ─── VXLAN tunnel per slave (uses eth0 for transport)
#       br_$$ ─── bridge connecting all VXLANs and sandbox veth
#       veth_host_$$ ─── bridge port, paired with sandbox veth
#     [sandbox netns]
#       veth_$$ (FIXED_MASTER_IP) ═══ private L2 via bridge+vxlan ═══ slave(s)
#
#   With 0 slaves, only PID and network namespace isolation is created
#   (no VXLAN, no bridge — just loopback in the sandbox).
#
# Environment variables:
#   SANDBOX_MASTER_IP         Fixed master IP inside sandbox. Default 10.99.0.254.
#   ISOLATED_SUBNET           Subnet prefix length. Defaults to 24.
#   ISOLATED_NET_IF           Host network interface for VXLAN underlay.
#                             Defaults to eth0.
#   CTEST_RESOURCE_GROUP_COUNT
#                             Set by CTest: number of allocated resource groups
#                             (= number of slaves). 0 or unset means no slaves.
#   CTEST_RESOURCE_GROUP_<N>_SLAVES
#                             Set by CTest: allocated slave resource per group
#                             (format: "id:<ip_underscored>,slots:1").
#   SSH_KEY                   Path to SSH private key for slave access.
#                             Defaults to
#                             /commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub

COMMAND="${*}"
echo "Executing in an isolated environment: ${COMMAND}"

# --- Configuration -----------------------------------------------------------

# Sandbox overlay IPs — must be on a different subnet than the Docker macvlan
# network so that VXLAN route setup cannot break underlay connectivity.
# Master: *.254, Slaves: *.1, *.2, *.3, ...
SANDBOX_NET="${SANDBOX_NET:-10.99.0}"
FIXED_MASTER_IP="${SANDBOX_MASTER_IP:-${SANDBOX_NET}.254}"
SUBNET_MASK="${ISOLATED_SUBNET:-24}"
NET_IF="${ISOLATED_NET_IF:-eth0}"
SSH_KEY="${SSH_KEY:-/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub}"
SSH_OPTS=(-o StrictHostKeyChecking=no -o BatchMode=yes -i "${SSH_KEY}")

# Shared directory paths (tmpfs-backed volume shared between master and slave containers).
SHARED_BASE="/home/test-shared"
SHARED_DIR="${SHARED_BASE}/isol_$$"
SHARED_FIXED_PATH="${TEST_SHARED_DIR:-/home/shared}"

# --- Determine allocated slaves -----------------------------------------------

# Number of slaves allocated by CTest (0 = no slaves needed).
NUM_SLAVES="${CTEST_RESOURCE_GROUP_COUNT:-0}"

# Extract actual Docker IPs of allocated slaves from CTest resource variables.
declare -a SLAVE_DOCKER_IPS=()
for ((i = 0; i < NUM_SLAVES; i++)); do
    varname="CTEST_RESOURCE_GROUP_${i}_SLAVES"
    raw="${!varname:-}"
    if [[ -z "$raw" ]]; then
        echo "ERROR: ${varname} not set but CTEST_RESOURCE_GROUP_COUNT=${NUM_SLAVES}" >&2
        exit 1
    fi
    ip=$(printf '%s' "$raw" | sed 's/id:\([^,]*\).*/\1/' | tr '_' '.')
    SLAVE_DOCKER_IPS+=("$ip")
done

# Master's own IP on the Docker network (for VXLAN local endpoint).
MASTER_ETH0_IP=$(ip -4 addr show "$NET_IF" | grep -oP '(?<=inet )\S+' | cut -d/ -f1 | head -1)

# --- Environment variable overrides ------------------------------------------

# Override IPs to sandbox values so tests communicate via the VXLAN overlay.
# bwrap inherits the environment, so exporting here is sufficient.
export TEST_IP_MASTER="${FIXED_MASTER_IP}"
if ((NUM_SLAVES >= 1)); then
    export TEST_IP_SLAVE="${SANDBOX_NET}.1"
fi
if ((NUM_SLAVES >= 2)); then
    export TEST_IP_SLAVE_SECOND="${SANDBOX_NET}.2"
fi

# --- Network namespace and interface setup ------------------------------------

NS_NAME="isol_$$"
BRIDGE_IF="br_$$"
VETH_HOST="vh_$$"
VETH_SANDBOX="vs_$$"

cleanup() {
    if ((NUM_SLAVES > 0)); then
        # Remove shared directory symlink from each slave and restore interface names.
        for ((i = 0; i < NUM_SLAVES; i++)); do
            timeout 10 ssh "${SSH_OPTS[@]}" -o ConnectTimeout=5 root@"${SLAVE_DOCKER_IPS[$i]}" "
                rm -f ${SHARED_FIXED_PATH}
                # Delete VXLAN (now named eth0) and restore original Docker eth0.
                ip link del eth0 2>/dev/null || true
                if ip link show underlay0 &>/dev/null; then
                    ip link set underlay0 down && ip link set underlay0 name eth0 && ip link set eth0 up
                fi
            " 2>/dev/null || true
        done

        # Remove bridge (detaches all ports and removes the bridge device).
        ip link del "$BRIDGE_IF" 2>/dev/null || true

        # Remove VXLAN devices from master namespace.
        for ((i = 0; i < NUM_SLAVES; i++)); do
            local_vni="${SLAVE_DOCKER_IPS[$i]##*.}"
            ip link del "vx_${local_vni}_$$" 2>/dev/null || true
        done

        # Remove the per-test shared directory.
        rm -rf "${SHARED_DIR}"
    fi

    # Remove network namespace (also removes veth_sandbox, which destroys the pair).
    ip netns del "$NS_NAME" 2>/dev/null || true
    return 0
}
trap cleanup EXIT

# --- Pre-cleanup: remove stale resources from previous aborted runs -----------

# Remove any leftover netns with the same name (possible PID recycling).
if ip netns list 2>/dev/null | grep -qw "$NS_NAME"; then
    ip netns del "$NS_NAME" 2>/dev/null || true
    # Wait for the kernel to finish unmounting the stale bind mount.
    while [[ -e "/run/netns/$NS_NAME" ]]; do sleep 0.01; done
fi

# Remove stale bridge and veth (from a killed previous run with same PID).
ip link del "$BRIDGE_IF" 2>/dev/null || true
ip link del "$VETH_HOST" 2>/dev/null || true

if ((NUM_SLAVES > 0)); then
    # Remove any stale master-side VXLAN devices for the same VNI(s).
    # Since a slave is exclusively allocated, any existing VXLAN with its VNI
    # on this host is guaranteed stale (from a killed previous run).
    for ((i = 0; i < NUM_SLAVES; i++)); do
        stale_vni="${SLAVE_DOCKER_IPS[$i]##*.}"
        for stale_dev in $(ip -o link show 2>/dev/null | grep -oP "vx_${stale_vni}_\d+" || true); do
            ip link del "$stale_dev" 2>/dev/null || true
        done
    done

    # Remove stale shared directory and slave-side symlinks.
    # Also restore eth0 if a previous run was killed before cleanup.
    rm -rf "${SHARED_DIR}"
    for ((i = 0; i < NUM_SLAVES; i++)); do
        timeout 5 ssh "${SSH_OPTS[@]}" -o ConnectTimeout=3 root@"${SLAVE_DOCKER_IPS[$i]}" "
            rm -f ${SHARED_FIXED_PATH}
            if ip link show underlay0 &>/dev/null; then
                ip link del eth0 2>/dev/null || true
                ip link set underlay0 down && ip link set underlay0 name eth0 && ip link set eth0 up
            fi
        " 2>/dev/null || true
    done
fi

# Create an isolated network namespace (retry briefly if a concurrent cleanup
# left a stale mount that the kernel hasn't fully released yet).
for attempt in 1 2 3 4 5; do
    if ip netns add "$NS_NAME" 2>/tmp/netns_add_err_$$; then
        break
    fi
    echo "WARNING: ip netns add $NS_NAME failed (attempt $attempt/5):" >&2
    cat /tmp/netns_add_err_$$ >&2
    echo "  /run/netns/$NS_NAME exists: $([[ -e "/run/netns/$NS_NAME" ]] && echo yes || echo no)" >&2
    echo "  /run/netns/ contents: $(ls -la /run/netns/ 2>&1)" >&2
    echo "  ip netns list: $(ip netns list 2>&1)" >&2
    echo "  mount | grep netns: $(mount 2>/dev/null | grep netns)" >&2
    # Clean up stale bind mount if present.
    ip netns del "$NS_NAME" 2>/dev/null || true
    while [[ -e "/run/netns/$NS_NAME" ]]; do sleep 0.01; done
    if ((attempt == 5)); then
        echo "ERROR: Failed to create network namespace $NS_NAME after 5 retries" >&2
        echo "  PID=$$, uid=$(id), capabilities=$(cat /proc/self/status | grep -i cap)" >&2
        rm -f /tmp/netns_add_err_$$
        exit 1
    fi
done
rm -f /tmp/netns_add_err_$$

# Narrow the ephemeral local port range so the kernel never hands out the fixed
# SOME/IP test ports as ephemeral source ports.
# The sysctl will try to write to /proc/sys/net/ipv4/ip_local_port_range, but docker
# mounts /proc/sys as read-only for non-privileged containers. As such, sysctl -w fails with
# EROFS ("Read-only file system"). To fix this, remount /proc/sys as read-write.
# The desired range is read from the container's current setting (configured via
# docker-compose.yaml) so it is applied identically inside the per-test netns.
LOCAL_PORT_RANGE="$(sysctl -n net.ipv4.ip_local_port_range | awk '{print $1, $2}')"
if ip netns exec "$NS_NAME" sh -c \
        'mount -o remount,rw /proc/sys 2>/dev/null
         sysctl -wq "net.ipv4.ip_local_port_range=$1"' _ "${LOCAL_PORT_RANGE}" &&
   [[ "$(ip netns exec "$NS_NAME" sysctl -n net.ipv4.ip_local_port_range | awk '{print $1, $2}')" \
      == "${LOCAL_PORT_RANGE}" ]]; then
    echo "Set local port range in netns $NS_NAME:" \
         "range=$(ip netns exec "$NS_NAME" sysctl -n net.ipv4.ip_local_port_range)" >&2
else
    echo "ERROR: Could not set local port range (${LOCAL_PORT_RANGE}) in netns $NS_NAME" >&2
    exit 1
fi

if ((NUM_SLAVES == 0)); then
    # --- No slaves: just isolated namespace with loopback ---------------------
    ip netns exec "$NS_NAME" ip link set lo up

else
    # --- Configure slaves and VXLAN tunnels -----------------------------------

    # Bridge connecting all VXLANs to sandbox veth.
    ip link add "$BRIDGE_IF" type bridge stp_state 0 forward_delay 0
    ip link set "$BRIDGE_IF" up

    for ((i = 0; i < NUM_SLAVES; i++)); do
        slave_docker_ip="${SLAVE_DOCKER_IPS[$i]}"
        slave_sandbox_ip="${SANDBOX_NET}.$((i + 1))"
        vni="${slave_docker_ip##*.}"
        vxlan_port=$((10000 + vni))
        vxlan_if="vx_${vni}_$$"

        # Verify that the slave is reachable.
        if ! ping -c 1 -W 3 "$slave_docker_ip" >/dev/null 2>&1; then
            echo "ERROR: Slave $((i + 1)) at ${slave_docker_ip} is not reachable." >&2
            exit 1
        fi

        # Set up VXLAN endpoint on the slave container.
        # After creating the VXLAN (using Docker's eth0 as underlay transport),
        # rename Docker's eth0 → underlay0 and the VXLAN → eth0. This gives
        # tests a well-known interface name, mirroring the master sandbox setup.
        # The VXLAN's underlay device binding is by interface index (not name),
        # so renaming is safe.
        ssh "${SSH_OPTS[@]}" -o ConnectTimeout=10 root@"${slave_docker_ip}" "
            ip link del vxlan_${vni} 2>/dev/null || true
            ip link add vxlan_${vni} type vxlan id ${vni} \
                remote ${MASTER_ETH0_IP} local ${slave_docker_ip} dstport ${vxlan_port} dev eth0
            ip link set vxlan_${vni} up
            # Rename: Docker eth0 → underlay0, VXLAN → eth0
            ip link set eth0 down && ip link set eth0 name underlay0 && ip link set underlay0 up
            ip link set vxlan_${vni} name eth0
            # Configure overlay addressing on the now-named eth0
            ip addr add ${slave_sandbox_ip}/${SUBNET_MASK} dev eth0 noprefixroute
            ip route replace ${FIXED_MASTER_IP}/32 dev eth0
            ip route replace ${SANDBOX_NET}.0/${SUBNET_MASK} dev eth0
            ip route replace multicast 224.0.0.0/4 dev eth0
        " || {
            echo "ERROR: Failed to configure VXLAN on slave $((i + 1)) at ${slave_docker_ip}" >&2
            exit 1
        }

        # VXLAN tunnel endpoint in master namespace, bound to eth0.
        ip link add "$vxlan_if" type vxlan \
            id "$vni" remote "$slave_docker_ip" local "$MASTER_ETH0_IP" \
            dstport "$vxlan_port" dev "$NET_IF"
        ip link set "$vxlan_if" up
        ip link set "$vxlan_if" master "$BRIDGE_IF"
    done

    # Veth pair connecting sandbox to bridge.
    ip link add "$VETH_HOST" type veth peer name "$VETH_SANDBOX"
    ip link set "$VETH_HOST" master "$BRIDGE_IF"
    ip link set "$VETH_HOST" up

    # Sandbox end moves to the isolated namespace.
    ip link set "$VETH_SANDBOX" netns "$NS_NAME"

    # Configure the sandbox's network interface.
    # Rename the veth to eth0 so tests can use a well-known interface name.
    ip netns exec "$NS_NAME" sh -c "
        ip link set lo up
        ip link set ${VETH_SANDBOX} name eth0
        ip link set eth0 up
        ip addr add ${FIXED_MASTER_IP}/${SUBNET_MASK} dev eth0 noprefixroute
        ip route add ${SANDBOX_NET}.0/${SUBNET_MASK} dev eth0
        ip route add multicast 224.0.0.0/4 dev eth0
    "

    # Create per-test shared directory and symlink it on each slave.
    mkdir -p "${SHARED_DIR}"
    for ((i = 0; i < NUM_SLAVES; i++)); do
        ssh "${SSH_OPTS[@]}" -o ConnectTimeout=10 root@"${SLAVE_DOCKER_IPS[$i]}" \
            "ln -sfn ${SHARED_DIR} ${SHARED_FIXED_PATH}"
    done

    # Verify connectivity through the tunnel to each slave.
    for ((i = 0; i < NUM_SLAVES; i++)); do
        slave_sandbox_ip="${SANDBOX_NET}.$((i + 1))"
        if ! ip netns exec "$NS_NAME" ping -c 1 -W 3 "$slave_sandbox_ip" >/dev/null 2>&1; then
            echo "WARNING: Cannot reach slave $((i + 1)) at ${slave_sandbox_ip} from sandbox" >&2
        fi
    done
fi

# Export the fixed shared dir path so tests can use it.
export TEST_SHARED_DIR="${SHARED_FIXED_PATH}" # re-export for bwrap child

# --- Execute test inside the sandbox ------------------------------------------

# Capture network traffic inside the sandbox (if PCAP_DIR is set).
TCPDUMP_PID=""
if [[ "${PCAP_DIR:-}" && "${CTEST_TEST_NAME:-}" ]]; then
    ip netns exec "$NS_NAME" \
        tcpdump -U -i any -w "${PCAP_DIR}/${CTEST_TEST_NAME}.pcap" 2>/dev/null &
    TCPDUMP_PID=$!
fi

# Run bwrap inside the pre-configured network namespace. Only PID isolation is
# added by bwrap (network is already isolated via the namespace).
# Build bwrap arguments. When slaves are present, bind-mount the per-test
# shared directory to the fixed path inside the sandbox.
BWRAP_EXTRA=()
if ((NUM_SLAVES > 0)); then
    BWRAP_EXTRA+=(--bind "${SHARED_DIR}" "${SHARED_FIXED_PATH}")
fi

TEST_RC=0

# Need to hide /run/netns before invocation of bwrap, otherwise it gets confused
# if another test deletes its namespace between enumeration and recursive bind mount.
ip netns exec "$NS_NAME" \
    unshare -m -- sh -c '
        mount -t tmpfs tmpfs /run/netns
        exec "$@"
    ' _ \
    bwrap \
    --unshare-pid \
    --die-with-parent \
    --bind / / \
    --dev /dev \
    --proc /proc \
    --perms 1777 --tmpfs /tmp \
    "${BWRAP_EXTRA[@]}" \
    -- \
    sh -c "${COMMAND}; retval=\$?; kill -s 9 -1 2>/dev/null; exit \$retval" ||
    TEST_RC=$?

# Stop tcpdump.
if [[ "$TCPDUMP_PID" ]]; then
    kill "$TCPDUMP_PID" 2>/dev/null || true
    wait "$TCPDUMP_PID" 2>/dev/null || true
fi

exit "$TEST_RC"
