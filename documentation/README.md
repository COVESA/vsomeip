# Multicast

To use IP Multicast, the route must be added.
This is needed to define a network route and enable boardnet (external) communication.
In Linux this can be done by:

```bash
ip route add 224.0.0.0/4 dev eth0
```

Other OSes may have different ways to do this.

## Diagrams

### Use cases Overview

![Use cases Overview](./diagrams/usecases_overview.drawio.svg)

### Use case Offer service

![Offer service](./diagrams/offer_service.drawio.svg)

### Sequence Offer service

![Seq Offer service](./diagrams/sequence_offer_service.puml.svg)
