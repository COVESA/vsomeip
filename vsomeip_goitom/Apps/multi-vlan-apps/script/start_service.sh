

#!/bin/bash
set -e  # exit if any command fails

# Determine the directory where this script lives

PROJ_DIR="/home/get/repo/vsomeip/vsomeip_goitom/Apps/vlan-apps-inv"
BUILD_DIR="${PROJ_DIR}/build"
CONFIG_DIR="${PROJ_DIR}/config"

export VSOMEIP_CONFIGURATION="${CONFIG_DIR}/vsomeip-service.json"

echo "Configuration file: $VSOMEIP_CONFIGURATION"
echo "Binary directory: $BUILD_DIR"
echo

echo "Starting RoutingManagerService..."
"${BUILD_DIR}/RoutingManagerService" &
ROUTING_PID=$!

echo "Waiting for routing manager to initialize..."
sleep 2

echo "Starting ServiceProviderApp..."
"${BUILD_DIR}/response-sample-170" &
SERVICE_PID=$!

echo "Both processes running."
echo "RoutingManagerService PID = $ROUTING_PID"
echo "ServiceProviderApp PID = $SERVICE_PID"

# Wait for both (this keeps the script running)
wait $ROUTING_PID $SERVICE_PID