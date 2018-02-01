# Configuration files
set(EXAMPLE_CONFIG_FILES
    "vsomeip.json"
    "vsomeip-local.json"
    "vsomeip-tcp-client.json"
    "vsomeip-tcp-service.json"
    "vsomeip-udp-client.json"
    "vsomeip-udp-service.json"
)

install(
    FILES ${EXAMPLE_CONFIG_FILES}
    DESTINATION etc/vsomeip
    COMPONENT config
)
