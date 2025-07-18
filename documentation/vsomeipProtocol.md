# vSomeIP Protocol Documentation

- [vSomeIP Protocol Documentation](#vsomeip-protocol-documentation)
  - [VSOMEIP\_ASSIGN\_CLIENT (0x00)](#vsomeip_assign_client-0x00)
  - [VSOMEIP\_ASSIGN\_CLIENT\_ACK (0x01)](#vsomeip_assign_client_ack-0x01)
  - [VSOMEIP\_REGISTER\_APPLICATION (0x02)](#vsomeip_register_application-0x02)
  - [VSOMEIP\_DEREGISTER\_APPLICATION (0x03)](#vsomeip_deregister_application-0x03)
  - [VSOMEIP\_APPLICATION\_LOST (0x04)](#vsomeip_application_lost-0x04)
  - [VSOMEIP\_ROUTING\_INFO (0x05)](#vsomeip_routing_info-0x05)
  - [VSOMEIP\_REGISTERED\_ACK (0x06)](#vsomeip_registered_ack-0x06)
  - [VSOMEIP\_PING (0x07)](#vsomeip_ping-0x07)
  - [VSOMEIP\_PONG (0x08)](#vsomeip_pong-0x08)
  - [VSOMEIP\_OFFER\_SERVICE (0x10)](#vsomeip_offer_service-0x10)
  - [VSOMEIP\_STOP\_OFFER\_SERVICE (0x11)](#vsomeip_stop_offer_service-0x11)
  - [VSOMEIP\_SUBSCRIBE (0x12)](#vsomeip_subscribe-0x12)
  - [VSOMEIP\_UNSUBSCRIBE (0x13)](#vsomeip_unsubscribe-0x13)
  - [VSOMEIP\_EXPIRE (0x2A)](#vsomeip_expire-0x2a)
  - [VSOMEIP\_REQUEST\_SERVICE (0x14)](#vsomeip_request_service-0x14)
  - [VSOMEIP\_RELEASE\_SERVICE (0x15)](#vsomeip_release_service-0x15)
  - [VSOMEIP\_SUBSCRIBE\_NACK (0x16)](#vsomeip_subscribe_nack-0x16)
  - [VSOMEIP\_SUBSCRIBE\_ACK (0x17)](#vsomeip_subscribe_ack-0x17)
  - [VSOMEIP\_SEND (0x18)](#vsomeip_send-0x18)
  - [VSOMEIP\_NOTIFY (0x19)](#vsomeip_notify-0x19)
  - [VSOMEIP\_NOTIFY\_ONE (0x1A)](#vsomeip_notify_one-0x1a)
  - [VSOMEIP\_REGISTER\_EVENT (0x1B)](#vsomeip_register_event-0x1b)
  - [VSOMEIP\_UNREGISTER\_EVENT (0x1C)](#vsomeip_unregister_event-0x1c)
  - [VSOMEIP\_ID\_RESPONSE (0x1D)](#vsomeip_id_response-0x1d)
  - [VSOMEIP\_ID\_REQUEST (0x1E)](#vsomeip_id_request-0x1e)
  - [VSOMEIP\_OFFERED\_SERVICES\_REQUEST (0x1F)](#vsomeip_offered_services_request-0x1f)
  - [VSOMEIP\_OFFERED\_SERVICES\_RESPONSE (0x20)](#vsomeip_offered_services_response-0x20)
  - [VSOMEIP\_UNSUBSCRIBE\_ACK (0x21)](#vsomeip_unsubscribe_ack-0x21)
  - [VSOMEIP\_RESEND\_PROVIDED\_EVENTS (0x22)](#vsomeip_resend_provided_events-0x22)
  - [VSOMEIP\_UPDATE\_SECURITY\_POLICY (0x23)](#vsomeip_update_security_policy-0x23)
  - [VSOMEIP\_UPDATE\_SECURITY\_POLICY\_INT (0x29)](#vsomeip_update_security_policy_int-0x29)
  - [VSOMEIP\_UPDATE\_SECURITY\_POLICY\_RESPONSE (0x24)](#vsomeip_update_security_policy_response-0x24)
  - [VSOMEIP\_REMOVE\_SECURITY\_POLICY (0x25)](#vsomeip_remove_security_policy-0x25)
  - [VSOMEIP\_REMOVE\_SECURITY\_POLICY\_RESPONSE    (0x26)](#vsomeip_remove_security_policy_response----0x26)
  - [VSOMEIP\_UPDATE\_SECURITY\_CREDENTIALS (0x27)](#vsomeip_update_security_credentials-0x27)
  - [VSOMEIP\_DISTRIBUTE\_SECURITY\_POLICIES (0x28)](#vsomeip_distribute_security_policies-0x28)
  - [VSOMEIP\_SUSPEND (0x30)](#vsomeip_suspend-0x30)
  - [VSOMEIP\_CONFIG (0x31)](#vsomeip_config-0x31)

Vsomeip protocol payload will contain a start and end tag with the following value: 0x67376d07 (the end tag is reversed).

## VSOMEIP_ASSIGN_CLIENT (0x00)

    Command            00
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    Name               xx ... xx        ;#xx = Size

## VSOMEIP_ASSIGN_CLIENT_ACK (0x01)

    Command            01
    Version            xx xx
    Client             xx xx
    Size               02 00 00 00
    Assigned           xx xx

## VSOMEIP_REGISTER_APPLICATION (0x02)

    Command            02
    Version            xx xx
    Client             xx xx
    Size               02 00 00 00
    Port               xx xx

## VSOMEIP_DEREGISTER_APPLICATION (0x03)

    Command            03
    Version            xx xx
    Client             xx xx
    Size               00 00 00 00


## VSOMEIP_APPLICATION_LOST (0x04)

`<unused>`


## VSOMEIP_ROUTING_INFO (0x05)

The VSOMEIP_ROUTING_INFO (0x05) command with subcommand RIE_ADD_CLIENT (0x0) is sent from the daemon to client applications and serves two purposes:

- If the client ID on the payload matches the ID of the receiving client, it serves to indicate to the client that it was successfully registered to the daemon.

- If the client ID on the payload is different from the ID of the receiving client, it serves to add the payload client on the VSOMEIP_ROUTING_INFO command to a map of `guests_` of the receiving client, which contains the route (ip and port) for that client. This `guests_` is later used to create a connection between the two clients. Note that this is the behaviour for provider applications, consumer applications will receive a RIE_ADD_SERVICE_INSTANCE (0x2) subcommand for this purpose

On both cases it also adds to the `known_clients_` map which later serves to create the local endpoint between the provider and consumer applications

        Command            05
        Version            xx xx
        Client             xx xx
        Size               xx xx xx xx
        Entries
            SubCommand     xx        ; RIE_ADD_CLIENT (0x0) or RIE_DEL_CLIENT (0x1)
            Size           xx xx xx xx
            Client         xx xx
            [Address]      xx .. xx    ; Size - sizeof(Client) - sizeof(Port)
            [Port]         xx xx

            SubCommand     xx        ; RIE_ADD_SERVICE_INSTANCE (0x2) or RIE_DEL_SERVICE_INSTANCE (0x4)
            Size           xx xx xx xx    ; Command size
            Size           xx xx xx xx    ; Client info size
            Client         xx xx
            [Address]      xx .. xx    ; Client info size - sizeof(Client) - sizeof(Port)
            [Port]         xx xx
            Size           xx xx xx xx    ; Services size
                Service    xx xx
                Instance   xx xx
                Major      xx
                Minor      xx xx xx xx


## VSOMEIP_REGISTERED_ACK (0x06)

    Command            06
    Version            xx xx
    Client             xx xx
    Size               00 00 00 00


## VSOMEIP_PING (0x07)

    Command            07
    Version            xx xx
    Client             00 00
    Size               00 00 00 00


## VSOMEIP_PONG (0x08)

    Command            08
    Version            xx xx
    Client             xx xx
    Size               00 00 00 00


## VSOMEIP_OFFER_SERVICE (0x10)

    Command            10
    Version            xx xx
    Client             xx xx
    Size               09 00 00 00
    Service            xx xx
    Instance           xx xx
    Major              xx
    Minor              xx xx xx xx


## VSOMEIP_STOP_OFFER_SERVICE (0x11)

    Command            11
    Version            xx xx
    Client             xx xx
    Size               09 00 00 00
    Service            xx xx
    Instance           xx xx
    Major              xx
    Minor              xx xx xx xx


## VSOMEIP_SUBSCRIBE (0x12)

    Command            12
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    Service            xx xx
    Instance           xx xx
    Eventgroup         xx xx
    Major              xx
    Event              xx xx
    Pending ID         xx xx
    Filter
        OnChange                xx
        OnChangeResetsInterval  xx
        Interval                xx xx xx xx xx xx xx xx
        Ignore (per entry)
            Key                 xx xx xx xx xx xx xx xx
            Value               xx


## VSOMEIP_UNSUBSCRIBE (0x13)

## VSOMEIP_EXPIRE (0x2A)

    Command            13/2A
    Version            xx xx
    Client             xx xx
    Size               0a 00 00 00
    Service            xx xx
    Instance           xx xx
    Eventgroup         xx xx
    Event              xx xx
    Pending ID         xx xx


## VSOMEIP_REQUEST_SERVICE (0x14)

    Command            14
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    Entries
        Service        xx xx
        Instance       xx xx
        Major          xx
        Minor          xx xx xx xx


## VSOMEIP_RELEASE_SERVICE (0x15)

    Command            15
    Version            xx xx
    Client             xx xx
    Size               04 00 00 00
    Service            xx xx
    Instance           xx xx


## VSOMEIP_SUBSCRIBE_NACK (0x16)

    Command            16
    Version            xx xx
    Client             xx xx
    Size               0c 00 00 00
    Service            xx xx
    Instance           xx xx
    Eventgroup         xx xx
    Subscriber         xx xx
    Event              xx xx
    ID                 xx xx


## VSOMEIP_SUBSCRIBE_ACK (0x17)

    Command            17
    Version            xx xx
    Client             xx xx
    Size               0c 00 00 00
    Service            xx xx
    Instance           xx xx
    Eventgroup         xx xx
    Subscriber         xx xx
    Event              xx xx
    ID                 xx xx


## VSOMEIP_SEND (0x18)

## VSOMEIP_NOTIFY (0x19)

## VSOMEIP_NOTIFY_ONE (0x1A)

    Command            18|19|1a
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    Instance           xx xx
    Reliable           xx        ; UDP (00) or TCP (01)
    Status             xx        ; CRC of E2E - protected messages
    Destination        xx xx     ; Client ID of the receiver
    Payload            xx ... xx


## VSOMEIP_REGISTER_EVENT (0x1B)

    Command            1b
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx    ; 10 + #eventgroups * 2
    Entries
        Service            xx xx
        Instance           xx xx
        Notifier           xx xx
        Type               xx    ; ET_EVENT (00), ET_SELECTIVE_EVENT(01) or ET_FIELD(02)
        Provided           xx    ; False (00) or True (01)
        Reliability        xx    ; UDP (00) or TCP (01)
        IsCyclic           xx
        Num Eventgroups    xx xx
        Entries
            Eventgroup     xx xx


## VSOMEIP_UNREGISTER_EVENT (0x1C)

    Command            1c
    Version            xx xx
    Client             xx xx
    Size               07 00 00 00
    Service            xx xx
    Instance           xx xx
    Notifier           xx xx
    Provided           xx


## VSOMEIP_ID_RESPONSE (0x1D)

`<unused>`


## VSOMEIP_ID_REQUEST (0x1E)

`<unused>`


## VSOMEIP_OFFERED_SERVICES_REQUEST (0x1F)

    Command            1f
    Version            xx xx
    Client             xx xx
    Size               01 00 00 00
    OfferType          xx (00 = LOCAL, 01 = REMOTE, 02 = ALL)


## VSOMEIP_OFFERED_SERVICES_RESPONSE (0x20)

    Command            20
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    OfferedServices
        Subcommand     xx    (00 = ADD CLIENT, 01 = ADD SERVICE INSTANCE, 02 = DELETE SERVICE INSTANCE, 03 = DELETE CLIENT)
        Size           xx xx xx xx
        ServiceInstances
            Service    xx xx
            Instance   xx xx
            Major      xx xx
            Minor      xx xx


## VSOMEIP_UNSUBSCRIBE_ACK (0x21)

    Command            21
    Version            xx xx
    Client             xx xx
    Size               08 00 00 00
    Service            xx xx
    Instance           xx xx
    Eventgroup         xx xx
    Id                 xx xx


## VSOMEIP_RESEND_PROVIDED_EVENTS (0x22)

    Command            22
    Version            xx xx
    Client             xx xx
    Size               04 00 00 00
    PendingOfferId     xx xx xx xx


## VSOMEIP_UPDATE_SECURITY_POLICY (0x23)

## VSOMEIP_UPDATE_SECURITY_POLICY_INT (0x29)

    Command            23/29
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    UpdateId           xx xx xx xx
    Policy             xx ... xx


## VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE (0x24)

    Command            24
    Version            xx xx
    Client             xx xx
    Size               04 00 00 00
    UpdateId           xx xx xx xx


## VSOMEIP_REMOVE_SECURITY_POLICY (0x25)

    Command            25
    Version            xx xx
    Client             xx xx
    Size               0c 00 00 00
    UpdateId           xx xx xx xx
    Uid                xx xx xx xx
    Gid                xx xx xx xx


## VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE    (0x26)

    Command            26
    Version            xx xx
    Client             xx xx
    Size               04 00 00 00
    UpdateId           xx xx xx xx


## VSOMEIP_UPDATE_SECURITY_CREDENTIALS (0x27)

    Command            27
    Version            xx xx
    Client             xx xx
    Size               xx xx xx xx
    Credentials
        Uid            xx xx xx xx
        Gid            xx xx xx xx


## VSOMEIP_DISTRIBUTE_SECURITY_POLICIES (0x28)

    Command            28
    Version            xx xx
    Client             xx xx xx xx
    Size               xx xx xx xx
    PoliciesCount      xx xx xx xx
    Policies
        Size           xx xx xx xx
        Data           xx ... xx


## VSOMEIP_SUSPEND (0x30)

    Command            30
    Version            xx xx
    Client             00 00
    Size               00 00 00 00


## VSOMEIP_CONFIG (0x31)

    Command                 31
    Version                 00 00
    Client                  xx xx
    Size                    xx xx xx xx
    Configurations
        Key Size            xx xx xx xx
        Key (string)        xx ... xx
        Value Size          xx xx xx xx
        Value (string)      xx ... xx
