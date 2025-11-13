# Getting Started

The Multi-VLAN feature is supported when the Multiple Routing Managers feature is enabled. This is done by building the vsomeip with "DENABLE_MULTIPLE_ROUTING_MANAGERS=true".

When a host participates in more than one logical Ethernet segment (e.g., VLANs 170 & 180), each with its own unicast/multicast space, vSomeIP allows multiple routing managers, each bound to a specific network interface and configuration context.

Each application defines:

a unique network name (e.g. VLAN170, VLAN180)
a dedicated routing name and socket endpoint (e.g. /tmp/VLAN170-0, /tmp/VLAN180-0)
its own unicast address, multicast group, and SD port
This ensures that each VLAN operates as an independent communication domain 



From the configuration and logs, we can summarize the following points:


Each routing manager (RM):

Runs as the local router for a given SOME/IP network namespace (/tmp/VLAN###‑0 sockets show their creation in the logs).
Announces, routes, and dispatches messages between applications belonging to the same VLAN.
Maintains its own service discovery instance (configured via a separate multicast address/port).
Is created on initialization (create_routing_root: Routing root @ /tmp/VLAN170-0 etc.).
Operates fully in Host mode (Instantiating routing manager [Host]).
Hence,  two routing managers:

/tmp/VLAN170-0 handles CLGW170 & SRVGW170
/tmp/VLAN180-0 handles CLGW180 & SRVGW180
…working concurrently, completely independent of one another, sharing only the same vSomeIP binary/runtime.