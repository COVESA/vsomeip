

#!/bin/bash
set -e  # exit if any command fails

# Determine the directory where this script lives
PROJ_DIR="/home/gett/repo/vsomeip/vsomeip_goitom/Apps/vlan-apps-inv"
BUILD_DIR="${PROJ_DIR}/build"
CONFIG_DIR="${PROJ_DIR}/config"

export VSOMEIP_CONFIGURATION="${CONFIG_DIR}/vsomeip-client.json"

echo "Configuration file: $VSOMEIP_CONFIGURATION"
echo "Binary directory: $BUILD_DIR"
echo

echo "Starting RoutingManagerClient..."
"${BUILD_DIR}/RoutingManagerClient" &
ROUTING_PID=$!

echo "Waiting for routing manager to initialize..."
sleep 2

echo "Starting ClientApp..."
"${BUILD_DIR}/request-sample-170" &
CLIENT_PID=$!

echo "Both processes running."
echo "RoutingManagerClient PID = $ROUTING_PID"
echo "ClientApp PID = $CLIENT_PID"

# Wait for both (this keeps the script running)
wait $ROUTING_PID $CLIENT_PID