# vSomeIP Configurations

**Updated to vSomeIP 3.5.5**

<`TBD`> tags means it still needs to be checked/done.


## Index
- [Logging](#logging)
- [Routing](#routing)
- [Applications](#applications)
- [Network](#network)
- [Shutdown](#shutdown)
- [Dispatching](#dispatching)
- [Payload Sizes](#payload-sizes)
- [Endpoint Queue Sizes](#endpoint-queue-sizes)
- [TCP Restart Settings](#tcp-restart-settings)
- [Permissions](#permissions)
- [Security](#security)
- [Tracing](#tracing)
- [UDP Receive Buffer Size](#udp-receive-buffer-size)
- [Service Discovery](#service-discovery)
- [nPDU Default Timings](#npdu-default-timings)
- [Services](#services)
- [Internal Services](#internal-services)
- [Clients](#clients)
- [Watchdog](#watchdog)
- [Local Clients Keepalive](#local-clients-keepalive)
- [Selective Broadcasts Support](#selective-broadcasts-support)
- [E2E](#e2e)
- [Debounce](#debounce)
- [Acceptances](#acceptances)
- [Secure Services](#secure-services)
- [Partitions](#partitions)
- [Suppress Events](#suppress-events)
- [Environment Variables](#environment-variables)
---

## Logging

- **logging** - Used to configure the log messages of vSomeIP
    - **console** - Specifies whether logging via console is enabled, valid values are `true` or `false`. The default value is `true`.
    - **file**:
        - **enable** - Specifies whether a log file should be created, valid values are `true` or `false`. The default value is `false`.
        - **path** - The absolute path of the log file. The default value is `/tmp/vsomeip.log`.
    - **dlt** - Specifies whether Diagnostic Log and Trace (DLT) is enabled, valid values are `true` or `false`. The default value is `false`.
    - **level** - Specifies the log level, valid values are `trace`, `debug`, `info`, `warning`, `error`, `fatal`. The default value is `info`.
    - **version** - Configures logging of the vsomeip version
        - **enable** - Enable or disable cyclic logging of vsomeip version, valid values are `true` or `false`. The default value is `true`.
        - **interval** - Configures interval in seconds to log the vsomeip version. The default value is `10` sec.
    - **memory_log_interval** - Configures interval in seconds in which the routing manager logs its used memory. Setting a value greater than zero enables the logging. The default value is `0` sec.
    - **status_log_interval** - Configures interval in seconds in which the routing manager logs its internal status. Setting a value greater than zero enables the logging. The default value is `0` sec.
    - **statistics**
        - **interval** - How often to report statistics data (received messages/events) in ms. The minimum possible interval is `1000`, for configured values below, 1000 will be used. The default value is `10000` ms (10sec).
        - **min-frequency** - Minimum frequency of reported events. The default value is `50` Hz.
        - **max-messages** - Maximum number of different messages that are reported. The default value is `50`.

<details><summary>Example of DLT logging</summary>

```
"logging" :
{
    "level" : "debug",
    "console" : "false",
    "file" : { "enable" : "false" },
    "dlt" : "true"
},
```
</details>

## Routing
- **routing** (optional) - Specifies the properties of the routing. Either a string that specifies the application that hosts the routing component or a structure that specifies all properties of the routing. If the routing is not specified, the first started application will host the routing component.
    - **host** - Properties of the routing manager.
        - **name** - Name of the application that hosts the routing component.
        - **uid** - User identifier of the process that runs the routing component. Must be specified if credential checks are enabled by check_credentials set to true.
        - **gid** - Group identifier of the process that runs the routing component. Must be specified if credential checks are enabled by check_credentials set to true.
        - **unicast** (optional) - The unicast address that shall be used by the routing manager, if the internal communication shall be done by using TCP connections.
        - **port** (optional) - The port that shall be used by the routing manager, if the internal communication shall be done by using TCP connections.
    - **guests** (optional) - Properties of all applications that do not host the routing component, if the internal communication shall be done using TCP connections.
        - **unicast** - The unicast address that shall be used by the applications to connect to the routing manager. If not set, the unicast address of the host entry is used.
        - **ports** - A set of port ranges that shall be used to connect to the routing manager per user identifier/group identifier. Either specify uid, gid and ranges, or only a set of port ranges. If uid and gid are not explicitly specified, they default to any. Each client application requires two ports, one for receiving messages from other applications and one to send messages to other applications.
            - **uid** - User identifier
            - **gid** - Group identifier
            - **ranges** - Set of port ranges. Each entry consists of a `first`, `last` pair that determines the first and the last port of a port range.
                - **first** - First port of a port range
                - **last** - Last port of a port range

            **Note:** Each configured port range must contain an even number of ports. If an even port number is configured to be the routing host port, the first port in the range must also be even. If an uneven port number is configured to be the routing host port, the first port in the range must also be uneven.

- **routing-credentials (deprecated)** - The UID / GID of the application acting as routing manager. (Must be specified if credentials checks are enabled using `check_credentials` set to `true` in order to successfully check the routing managers credentials passed on connect)
    - **uid** - The routing managers UID.
    - **gid** - The routing managers GID.

<details><summary>Example of simple routing configuration</summary>

```
"routing" : "service-sample",
```
</details>

<details><summary>Example of routing using internal TCP Communication (1)</summary>

```
"routing" : {
    "host" : {
        "name" : "service-sample",
        "unicast" : "10.0.3.1",
        "port" : "31000"
    },
    "guests" : {
        "unicast" : "10.0.3.1",
        "ports" :
        [
            {
                "first" : "1026",
                "last" : "65535"
            }
        ]
    }
},
```
</details>

<details><summary>Example of routing using internal TCP Communication (2)</summary>

```
"routing": {
    "host": {
        "name": "service-sample",
        "unicast": "10.0.3.1",
        "port": "31000"
    },
    "guests": {
        "unicast": "10.0.3.1",
        "ports": [
        {
            "uid": 8205,
            "gid": 8205,
            "ranges": [
            {
                "first": 10000,
                "last": 10019
            }
            ]
        }
        ]
    }
},
```
</details>

## Applications

- **applications** (array) - Contains the applications of the host system that use this config file.
    - **name** - The name of the application.
    - **id** - The id of the application. Usually its high byte is equal to the diagnosis address. In this case the low byte must be different from zero. Thus, if the diagnosis address is 0x63, valid values range from 0x6301 until 0x63FF. It is also possible to use id values with a high byte different from the diagnosis address.
    - **max_dispatchers** (optional) - The maximum number of threads that shall be used to execute the application callbacks. The default value is `10`.
    - **max_dispatch_time** (optional) - The maximum time in ms that an application callback may consume before the callback is considered to be blocked (and an additional thread is used to execute pending callbacks if max_dispatchers is configured greater than 0). The default value if not specified is `100` ms.
    - **max_detached_thread_wait_time** (optional) - The maximum time in seconds that an application will wait for a detached dispatcher thread to finish executing. The default value if not specified is `5` sec.
    - **threads** (optional) - The number of internal threads to process messages and events within an application. Valid values are `1-255`. The default value is `2`.
    - **io_thread_nice** (optional) - The nice level for internal threads processing messages and events. POSIX/Linux only. For actual values refer to nice() documentation. The default value is `0`.
    - **request_debounce_time** (optional) - Specifies a debounce-time interval in ms in which request-service messages are sent to the routing manager. If an application requests many services in short same time the load of sent messages to the routing manager and furthermore the replies from the routing manager (which contains the routing info for the requested service if available) can be heavily reduced. The default value if not specified is `10` ms.
    - **plugins** (optional array) - Contains the plug-ins that should be loaded to extend the functionality of vsomeip.
        - **name** - The name of the plug-in.
        - **type** - The plug-in type (valid values: `application_plugin`). An application plug-in extends the functionality on application level. It gets informed by vsomeip over the basic application states (INIT/START/STOP) and can, based on these notifications, access the standard "application"-API via the runtime.
        - **additional** - Generic way to define configuration data for plugins.
    - **debounce** - Client/Application specific configuration of debouncing.
    - **has_session_handling** - Configures the session handling. Mostly used for E2E use cases when the application handles the CRC calculation over the SOME/IP header by themself, and need the ability to switch off the session handling as otherwise their calculated checksum does not match reality after vsomeip inserts the session identifier. Valid values are `true` or `false`. The default value is `true`.
    - **event_loop_periodicity** (optional) - If set to a positive value, it enables the io_context object's event processing run_for implementation to run the loop based on duration instead of running it until the work queue has work to be done. The default value is `0` seconds, which uses the io_context run interface.

<details><summary>Example of Applications configuration</summary>

```
"applications" :
[
    {
        "name" : "service-sample",
        "id" : "0x1277",
        "threads" : "4",
        "io_thread_nice" : "-5",
        "plugins" :
        [
            {
                "application_plugin" : "vsomeip-hello-plugin",
                "application_plugin" : "vsomeip-world-plugin",
            }
        ]
    },
    {
        "name" : "client-sample",
        "id" : "0x1344"
    },
    {
        "name": "other-client",
        "max_dispatchers": "0",
        "max_dispatch_time": "500",
        "max_detached_thread_wait_time": "6"
    },
],
```
</details>

## Network

- **network** (optional) - Network identifier used to support multiple routing managers on one host. This setting changes the name of the unix domain sockets in `/tmp/`. The default value is `vsomeip`, meaning the unix domain sockets will be named `/tmp/vsomeip-$CLIENTID`
- **diagnosis** - The diagnosis address (byte) that will be used to build client identifiers. The diagnosis address is assigned to the most significant byte in all client identifiers if not specified otherwise (for example through a predefined client ID). The default value is `0x01`.
- **diagnosis_mask** - The diagnosis mask (2 byte) is used to control the maximum amount of allowed concurrent vsomeip clients on an ECU and the start value of the client IDs.

    The default value is `0xFF00` meaning the most significant byte of the client ID is reserved for the diagnosis address and the client IDs will start with the diagnosis address as specified. The maximum number of clients is 255 as the Hamming weight of the inverted mask is 8 (2^8 = 256 - 1 (for the routing manager) = 255). The resulting client ID range with a diagnosis address of for example 0x45 would be 0x4501 to 0x45ff.

    Setting the mask to `0xFE00` doubles client ID range to 511 clients as the Hamming weight of the inverted mask is greater by one. With a diagnosis address of 0x45 the start value of client IDs is 0x4401 as bit 8 in 0x4500 is masked out. This then yields a client ID range of 0x4400 to 0x45ff.
- **unicast** (optional) - The IP address of the host system.
- **netmask** (optional) - The netmask to specify the subnet of the host system.
- **device** (optional) - If specified, IP endpoints will be bound to this device.

<details><summary>Example of Network configuration!</summary>

```
"unicast" : "10.10.3.1",
"netmask" : "255.255.255.0",
"diagnosis" : "0x63",
"diagnosis_mask" : "0xF300",
```
</details>

## Shutdown

- **shutdown_timeout** - Configures the time in milliseconds local clients wait for acknowledgement of their deregistration from the routing manager during shutdown. The default value is `5000` ms.

<details><summary>Example of Shutdown configuration</summary>

```
"shutdown_timeout" : 1000
```
</details>

## Dispatching

Define default settings for the maximum number of (additional) dispatchers and the maximum dispatch time. These default values are overwritten by application specific definitions.

- **dispatching** (optional)
    - **max_dispatchers** (optional) - The maximum number of threads that shall be used to execute the application callbacks. The default value is `10`.
    - **max_dispatch_time** (optional) - The maximum time in ms that an application callback may consume before the callback is considered to be blocked (and an additional thread is used to execute pending callbacks if max_dispatchers is configured greater than 0). The default value if not specified is `100` ms.

## Payload Sizes

- **payload-sizes** (array) - Array to limit the maximum allowed payload sizes per IP and port. If not specified otherwise the allowed payload sizes are `unlimited`. The settings in this array only affect communication over TCP. To limit the local payload size `max-payload-size-local` can be used.
    - **unicast** - IP Address of:
        - `On client side`: the IP of the remote service for which the payload size should be limited.
        - `On service side`: the IP of the offered service for which the payload size for receiving and sending should be limited.
    - **ports** (array) - Array which holds pairs of port and payload size statements.
        - **port** - The ports regarding:
            - `On client side`: the port of the remote service for which the payload size should be limited.
            - `On service side`: the port of the offered service for which the payload size for receiving and sending should be limited.
        - **max-payload-size** - The payload regarding:
            - `On client side`: the payload size limit in bytes of a message sent to the remote service hosted on beforehand specified IP and port.
            - `On service side`: the payload size limit in bytes of messages received and sent by the service offered on previously specified IP and port. If multiple services are hosted on the same port they all share the limit specified.
- **max-payload-size-local** - The maximum allowed payload size for node internal communication in bytes. By default the payload size for node internal communication is `unlimited`. It can be limited via this setting.
- **max-payload-size-reliable** - The maximum allowed payload size for TCP communication in bytes. By default the payload size for TCP communication is `unlimited`. It can be limited via this setting.
- **max-payload-size-unreliable** - The maximum allowed payload size for UDP communication via SOME/IP-TP in bytes. By default the payload size for UDP via SOME/IP-TP communication is `unlimited`. It can be limited via this setting. This setting only applies for SOME/IP-TP enabled methods/events/fields (otherwise the UDP default of 1400 bytes applies). See SOME/IP-TP for an example configuration.
- **buffer-shrink-threshold** - The number of processed messages which are half the size or smaller than the allocated buffer used to process them before the memory for the buffer is released and starts to grow dynamically again. This setting can be useful in scenarios where only a small number of the overall messages are a lot bigger then the rest and the memory allocated to process them should be released in a timely manner. If the value is set to zero the buffer sizes aren't reset and are as big as the biggest processed message. The default value is `5`.

    - **Example**: `buffer-shrink-threshold` is set to `50`.

        A message with 500 bytes has to be processed and the buffers grow accordingly. After this message 50 consecutive messages smaller than 250 bytes have to be processed before the buffer size is reduced and starts to grow dynamically again.

<details><summary>Example of Payload Sizes configuration</summary>

```
"max-payload-size-local" : "52428800",
"max-payload-size-reliable" : "52428800",
"payload-sizes":
[
    {
        "unicast":"10.10.3.1",
        "ports":
        [
            {
                "port":"30501",
                "max-payload-size":"614400"
            },
            {
                "port":"32506",
                "max-payload-size":"1572864"
            }
        ]
    }
],
```
</details>

## Endpoint Queue Sizes

- **endpoint-queue-limits** (array) - Array to limit the maximum allowed size in bytes of cached outgoing messages per IP and port (message queue size per endpoint). If not specified otherwise the allowed queue size is `unlimited`. The settings in this array only affect external communication. To limit the local queue size `endpoint-queue-limit-local` can be used.
    - **unicast**
        - `On client side`: The IP of the remote service for which the queue size of sent requests should be limited.
        - `On service side`: The IP of the offered service for which the queue size for sent responses should be limited. This IP address is therefore identical to the IP address specified via unicast setting on top level of the json file.
    - **ports** (array) - Array which holds pairs of port and queue size statements.
        - **port**
            - `On client side`: the port of the remote service for which the queue size of sent requests should be limited.
            - `On service side`: the port of the offered service for which the queue size for send responses should be limited.
        - **queue-size-limit**
            - `On client side`: the queue size limit in bytes of messages sent to the remote service hosted on beforehand specified IP and port.
            - `On service side`: the queue size limit in bytes for responses sent by the service offered on previously specified IP and port. If multiple services are hosted on the same port they all share the limit specified.
- **endpoint-queue-limit-external** - Setting to limit the maximum allowed size in bytes of cached outgoing messages for external communication (message queue size per endpoint). By default the queue size for external communication is `unlimited`. It can be limited via this setting. Settings done in the `endpoint-queue-limits` array override this setting.
- **endpoint-queue-limit-local** - Setting to limit the maximum allowed size in bytes of cached outgoing messages for local communication (message queue size per endpoint). By default the queue size for node internal communication is `unlimited`. It can be limited via this setting.


## TCP Restart Settings

- **tcp-restart-aborts-max** - Setting to limit the number of TCP client endpoint restart aborts due to unfinished TCP handshake. After the limit is reached, a forced restart of the TCP client endpoint is done if the connection attempt is still pending. The default value is `5`.
- **tcp-connect-time-max** - Setting to define the maximum time until the TCP client endpoint connection attempt should be finished. If tcp-connect-time-max is elapsed, the TCP client endpoint is forcefully restarted if the connection attempt is still pending. The default value is `5000` ms.

## Permissions

- **file-permissions**
    - **permissions-uds** - If UDS is used, this configures the user file-creation mode mask (umask) for the permissions of the sockets. The default value is `0666`.

## Security

vSomeIP has a security implementation based on UNIX credentials.

If activated every local connection is authenticated during connect using the standard UNIX credential passing mechanism.
During authentication a client transfers its client identifier together with its credentials (UID / GID) to the server which is then matched against the configuration.

If the received credentials don't match the policy, the socket will be immediately closed by the server and a message is logged.
If accepted the client identifier is bound to the receiving socket, and can therefore be used to do further security checks on incoming messages (vSomeIP messages as well as internal commands).

In general clients can be configured to be allowed/denied to request (means communicate with) and offer different service instances.
Every incoming vSomeIP message (request/response/notification) as well as offer service requests or local subscriptions are then checked against the policy.
If an incoming vsomeip message or another operation (e.g. offer/subscribe) violates the configured policies it is skipped and a message is logged.

Furthermore if an application receives information about other clients/services in the system, it must be received from the authenticated routing manager.
This is to avoid malicious applications faking the routing manager and therefore being able to wrongly inform other clients about services running on the system.

Therefore, whenever the "security" tag is specified, the routing manager (e.g. routingmanagerd/vsomeipd) must be a configured application with a fixed client identifier.

Credential passing is only possible via Unix-Domain-Sockets and therefore only available for local communication.
However if security is activated method calls from remote clients to local services are checked as well which means remote clients needs to be explicitly allowed.
Such a policy looks same in case for local clients except the `credentials` tag can be skipped.

The available configuration switches for the security feature are:

- **security** (optional) - If specified the credential passing mechanism is activated. However no credential or security checks are done as long as `check_credentials` isn't set to `true`, but the routing manager client ID must be configured if security tag is specified and shall not be set to 0x6300.
If `check_credentials` is set to `true`, the routing managers UID and GID needs to be specified using `routing-credentials` tag.
- **NOTE**: As long as no `security` node exists, the security implementation is switched off. This also means, no external security library will be loaded and used.

    If the `security` field is left empty, vSomeIP will treat it as external security. In this case, an external library, such as `libvsomeip-sec.so.1`, will be loaded automatically. A vSomeIP info log message will indicate the library being utilized (e.g., `[info] Using external security implementation!`).

    - **check_credentials** (optional) - Specifies whether security checks are active or not. This includes credentials checks on connect as well as all policies checks configured in follow, valid values are `true` or `false`. The default value is `false`. **NOTE**: vsomeip's security implementation can be put in a so called `Audit Mode` where all security violations will be logged but allowed. This mode can be used to build a security configuration. To activate the `Audit Mode` the 'security' object has to be included in the json file but the `check_credentials` switch has to be set to `false`.
    - **allow_remote_clients** (optional) - Specifies whether incoming remote requests / subscriptions are allowed to be sent to a local proxy / client. If not specified, all remote requests / subscriptions are allowed to be received by default. Valid values are `true` and `false`. The default value is `true`.
    - **policies** (array) - Specifies the security policies. Each policy at least needs to specify `allow` or `deny`.
        - **credentials** - Specifies the credentials for which a security policy will be applied. If `check_credentials` is set to `true` the credentials of a local application needs to be specified correctly to ensure local socket authentication can succeed.
            - **uid** - Specifies the LINUX User ID of the client application as decimal number. As a wildcard "any" can be used.
            - **gid** - Specifies the LINUX Group ID of the client application as decimal number. As a wildcard "any" can be used.
            - **allow** / **deny** (optional) - Specifies whether the LINUX user and group ids are allowed or denied for the policy.
                - **uid** (array) - Specifies a list of LINUX user ids. These may either be specified as decimal numbers or as ranges. Ranges are specified by the first and the last valid id (see example below).
                - **gid** (array) - Specifies a list of LINUX group ids. These may either be specified as decimal numbers or as ranges. Ranges are specified by the first and the last valid id (see example below).
        - **allow** / **deny** - This tag specifies either `allow` or `deny` depending on white or blacklisting is needed. Specifying `allow` and `deny` entries in one policy is therefore not allowed.
            - With **`allow`** a whitelisting of what is allowed can be done which means an empty `allow` tag implies everything is denied.
            - With **`deny`** a blacklisting of what is allowed can be done which means an empty `deny` tag implies everything is allowed.
            - **requests** (array) - Specifies a set of service instance pairs which the above client application using the credentials above is allowed/denied to communicate with.
                - **service** - Specifies a service for the `requests`.
                - **instance** (deprecated) - Specifies a instance for the `requests` As a wildcard "any" can be used which means a range from instance ID 0x01 to 0xFFFF which also implies a method ID range from 0x01 to 0xFFFF.
                - **instances** (array) - Specifies a set of instance ID and method ID range pairs which are allowed/denied to communicate with. If the `ids` tag below is not used to specify allowed/denied requests on method ID level one can also only specify a a set of instance ID ranges which are allowed/denied to be requested analogous to the allowed/denied `offers` section. If no method IDs are specified, the allowed/denied methods are by default a range from 0x01 to 0xFFFF.
                    - **ids** - Specifies a set of instance ID ranges which are allowed/denied to communicate with. It is also possible to specify a single instance ID as array element without giving an upper / lower range bound. As a wildcard "any" can be used which means a range from instance ID 0x01 to 0xFFFF.
                        - **first** - The lower bound of the instance range.
                        - **last** - The upper bound of the instance range.
                    - **methods** - Specifies a set of method ID ranges which are allowed/denied to communicate with. It is also possible to specify a single method ID as array element without giving an upper / lower range bound. As a wildcard "any" can be used which means a range from method ID 0x01 to 0xFFFF.
                        - **first** - The lower bound of the method range.
                        - **last** - The upper bound of the method range.
            - **offers** (array) - Specifies a set of service instance pairs which are allowed/denied to be offered by the client application using the credentials above.
                - **service** - Specifies a service for the _offers_.
                - **instance** (deprecated) - Specifies a instance for the _offers_. As a wildcard "any" can be used which means a range from instance ID 0x01 to 0xFFFF.
                - **instances** (array) - Specifies a set of instance ID ranges which are allowed/denied to be offered by the client application using the credentials above. It is also possible to specify a single instance ID as array element without giving an upper / lower range bound. As a wildcard "any" can be used which means a range from instance ID 0x01 to 0xFFFF.
                    - **first** - The lower bound of the instance range.
                    - **last** - The upper bound of the instance range.


<details><summary>Security configuration example</summary>

The `config` folder contains some addition vsomeip configuration files to run the vsomeip examples with activated security checks.
Additionally there's a security test in the `test/` subfolder which can be used for further reference.
They give a basic overview how to use the security related configuration tags described in this chapter to run a simple request/response or subscribe/notify example locally or remotely.

```
"security" :
{
    ...
    "policies" :
    [
        {
            ...
            "credentials" :
            {
                "uid" : "44",
                "gid" : "any"
             },
             "allow" :
             [
                 "requests" :
                 [
                     {
                         "service" : "0x6731",
                         "instance" : "0x0001"
                     }
                 ]
             ]
         },
         {
            "credentials" :
            {
                "deny" :
                [
                    {
                        "uid" : [ "1000", { "first" : "1002", "last" : "max" }],
                        "gid" : [ "0", { "first" : "100", "last" : "243" }, "300"]
                    },
                    {
                        "uid" : ["55"],
                        "gid" : ["55"]
                    }
                 ]
             },
             "allow" :
             [
                 "offers" :
                 [
                     {
                        "service" : "0x6728",
                        "instances" : [ "0x0001", { "first" : "0x0003", "last" : "0x0007" }, "0x0009"]
                     },
                     {
                        "service" : "0x6729",
                        "instances" : ["0x88"]
                     },
                     {
                        "service" : "0x6730",
                        "instance" : "any"
                     }
                 ],
                 "requests" :
                 [
                     {
                         "service" : "0x6732",
                         "instances" :
                         [
                             {
                                 "ids" : [ "0x0001", { "first" : "0x0003", "last" : "0x0007" }],
                                 "methods" : [ "0x0001", "0x0003", { "first" : "0x8001", "last" : "0x8006" } ]
                             },
                             {
                                 "ids" : [ "0x0009" ],
                                 "methods" : "any"
                             }
                         ]
                     },
                     {
                        "service" : "0x6733",
                        "instance" : "0x1"
                     },
                     {
                        "service" : "0x6733",
                        "instances" : [ "0x0002", { "first" : "0x0003", "last" : "0x0007" }, "0x0009"]
                     }
                 ]
             ]
         }
     ]
}
```
</details>

### Security Policy Extensions

vsomeip policy extension configuration supports the definition of paths that contain additional security policies to be loaded whenever a client with a yet unknown hostname connects to a local server endpoint.
The following configuration parameters are available and can be defined in a file named `vsomeip_policy_extensions.json`.

- **container_policy_extensions** (optional array) - Specifies the additional configuration folders to be loaded for each container hostname / filesystem path pair.
    - **container** - Specifies the linux hostname.
    - **path** - Specifies a filesystem path (relative to vsomeip_policy_extensions.json or absolute) which contains `$UID_$GID` subfolders that hold a `vsomeip_security.json` file. **NOTE**: (`$UID` / `$GID` is the UID /GID of the vsomeip client application to which a client from hostname defined with `container`connects to.


- **security-update-whitelist** - <`TBD`>
    - **uids** - Range of possible `uids` to use. Possible values are `any`, or range based using the tags `first` and `last`. The range based possible values are `dec/hex` values or `min/max` respectively.
    - **services** - Range of possible `services` to use. Possible values are `any`, or range based using the tags `first` and `last`. The range based possible values are `dec/hex` values or `min/max` respectively.
    - **check-whitelist** - Enables or disables the whitelist. Possible values are `true` or `false`.

<details><summary>Security policy extensions example</summary>

```
{
    "container_policy_extensions": [
        {
            "container": "8f61f1886ad7",
            "path": "/workspaces/network-workspace/vsomeip_ext/android"
        },
        {
            "container": "9e21356d92ef",
            "path": "/workspaces/network-workspace/vsomeip_ext/android"
        }
    ]
}
```
</details>

## Tracing

The Trace Connector is used to forward the internal messages that are sent over the Unix Domain Sockets (UDS) to DLT. Thus, it requires that DLT is installed and the DLT module can be found in the context of CMake.

- **tracing** (optional)
    - **enable** - Specifies whether the tracing of the SOME/IP messages is enabled, valid values are `true`, `false`. The default value is `false`. If tracing is enabled, the messages will be forwarded to DLT by the Trace Connector.
    - **sd_enable** - Specifies whether the tracing of the SOME/IP service discovery messages is enabled, valid values are `true`, `false`. The default value is `false`.
    - **channels** (array)(optional) - Contains the channels to DLT.
    **NOTE**: You can set up multiple channels to DLT over that you can forward the messages.
        - **name** - The name of the channel.
        - **id** - The id of the channel.
    - **filters** (array)(optional) - Contains the filters that are applied on the messages.
    **NOTE**: You can apply filters respectively filter rules on the messages with specific criteria and expressions. So only the filtered messages are forwarded to DLT.
        - **channel** (optional) - The id of the channel over that the filtered messages are forwarded to DLT. If no channel is specified the default channel (`TC`) is used. If you want to use a filter in several different channels, you can provide an array of channel ids.
        **NOTE**: If you use a positive filter with multiple channels, the same message will be forwarded multiple times to DLT.
        - **matches** (optional) - Specification of the criteria to include/exclude a message into/from the trace. You can either specify lists (array) or ranges of matching elements. A list may contain single identifiers which match all messages from/to all instances of the corresponding service or tuples consisting of service, instance and method-identifier. 'any' may be used as a wildcard for matching all services, instances or methods. A range is specified by two tuples "from" and "to", each consisting of service-, instance-and method-identifier. All messages with service-, instance-and method-identifiers that are greater than or equal to "from" and less than or equal to "to" are matched.
        - **type** (optional) - Specifies the filter type (valid values: `positive`, `negative`, `header-only`). The default value is `positive`.
            - A **positive filter** is used and a message matches one of the filter rules, the message will be traced/forwarded to DLT.
            - A **negative filter** messages can be excluded. So when a message matches one of the filter rules, the message will not be traced/forwarded to DLT.
            - A **header-only filter** is a positive filter that does not trace the message payload.

<details><summary>Example 1 (Minimal Configuration)!</summary>
This is the minimal configuration of the Trace Connector.

This just enables the tracing and all of the sent internal messages will be traced/forwarded to DLT.

```
"tracing" :
{
    "enable" : "true"
},
```
</details>

<details><summary>Example 2 (Using Filters)!</summary>
As it is a positive filter, the example filter ensures that only messages representing method '0x80e8' from instances of service '0x1234' will be forwarded to the DLT.

If it was specified as a negative filter, all messages except messages representing method '0x80e8' from instances of service
'0x1234' would be forwarded to the DLT.

The general filter rules are:
* The default filter is a positive filter for all messages.
* The default filter is active on a channel as long as no other positive filter is specified.
* Negative filters block matching messages. Negative filters overrule positive filters. Thus, as soon as a messages matches a negative filter it will not be forwarded.
* The identifier '0xffff' is a wildcard that matches any service, instance or method. The keyword 'any' can be used as a replacement for '0xffff'.
* Wildcards must not be used within range filters.
```
"tracing" :
{
    "enable" : "true",
    "channels" :
    [
        {
            "name" : "My channel",
            "id" : "MC"
        }
    ],
    "filters" : [
        {
            "channel" : "MC",
            "matches" : [ { "service" : "0x1234", "instance" : "any", "method" : "0x80e8" } ],
            "type" : "positive"
        }
    ]
},
```
</details>

<details><summary>Other example!</summary>

```
"tracing": {
    "filters": [{
        "channel": "TC",
        "services": ["0xb05a", "0xb05b", "0xb05c", "0xb05d", "0xb05e", "0xb0be"],
        "type": "negative"
    },{
        "channel": "TC",
        "matches" : [
            { "service" : "0x1b58", "instance" : "any", "method" : "0x8001" },
            { "service" : "0x1b58", "instance" : "any", "method" : "0x8002" },
            { "service" : "0x1b58", "instance" : "any", "method" : "0x8003" },
            { "service" : "0x3505", "instance" : "any", "method" : "0x8001" },
            { "service" : "0xd540", "instance" : "any", "method" : "0x8002" },
            { "service" : "0xd540", "instance" : "any", "method" : "0x8004" },
            { "service" : "0xb04e", "instance" : "any", "method" : "0x8001" },
            { "service" : "0xb0be", "instance" : "any", "method" : "0x0002" },
            { "service" : "0xe007", "instance" : "any", "method" : "0x8001" }
        ],
        "type": "header-only"
    },{
        "channel": "TC",
        "matches": [
            { "service": "0xfb67", "instance": "0x0080", "method": "0x0005" },
            { "service": "0xf8c9", "instance": "any", "method": "0x000f" },
            { "service": "0xf8c9", "instance": "any", "method": "0x0010" }
        ],
        "type": "negative"
    }]
}
```

</details>


## UDP Receive Buffer Size

- **udp-receive-buffer-size** - Specifies the size of the socket receive buffer (SO_RCVBUF) used for UDP client and server endpoints in bytes. Requires CAP_NET_ADMIN to be successful. The default value is: `1703936`.


## Service Discovery

- **service-discovery** - Contains settings related to the Service Discovery of the host application.
    - **enable** - Specifies whether the Service Discovery is enabled, valid values are `true`, `false`. The default value is `true`.
    - **initial_state** - Specifies the initial Service Discovery state after startup. Valid values are `unknown`, `suspended` or `resumed`. The default value is `unknown`.
    - **multicast** - The multicast address which the messages of the Service Discovery will be sent to. The default value is `224.224.224.0`.
    - **port** - The port of the Service Discovery. The default value is `30490`.
    - **protocol** The protocol that is used for sending the Service Discovery messages, valid values are `tcp`, `udp`. The default value is `udp`.
    - **initial_delay_min** - Minimum delay before first offer message. The default value is `0` ms.
    - **initial_delay_max** - Maximum delay before first offer message. The default value is `3000` ms.
    - **repetitions_base_delay** - Base delay sending offer messages within the repetition phase. The default value is `10`.
    - **repetitions_max** - Maximum number of repetitions for provided services within the repetition phase. The default value is `3`.
    - **ttl** - Lifetime of entries for provided services as well as consumed services and eventgroups. The default value is `0xFFFFFF`, until next reboot.
    - **ttl_factor_offers** (optional array) - Array which holds correction factors for incoming remote offers. If a value greater than one is specified for a service instance, the TTL field of the corresponding service entry will be multiplied with the specified factor. **Example**: An offer of a service is received with a TTL of 3 sec and the TTL factor is set to 5. The remote node stops offering the service w/o sending a StopOffer message. The service will then expire (marked as unavailable) 15 seconds after the last offer has been received.
        - **service** - The id of the service.
        - **instance** - The id of the service instance.
        - **ttl_factor** - TTL correction factor
    - **ttl_factor_subscriptions** (optional array) - Array which holds correction factors for incoming remote subscriptions. If a value greater than one is specified for a service instance, the TTL field of the corresponding eventgroup entry will be multiplied with the specified factor. **Example**: A remote subscription to an offered service is received with a TTL of 3 sec and the TTL factor is set to 5. The remote node stops resubscribing to the service w/o sending a StopSubscribeEventgroup message. The subscription will then expire 15 seconds after the last resubscription has been received.
        - **service** - The id of the service.
        - **instance** - The id of the service instance.
        - **ttl_factor** - TTL correction factor
    - **cyclic_offer_delay** - Cycle of the OfferService messages in the main phase. The default value is `1000` ms.
    - **request_response_delay** - Minimum delay of a unicast message to a multicast message for provided services and eventgroups. The default value is `2000` ms.
    - **offer_debounce_time** - Time which the stack collects new service offers before they enter the repetition phase. This can be used to reduce the number of sent messages during startup. The default value is `500` ms.
    - **find_debounce_time** - Time which the stack collects non local service requests before sending find messages. The default value is `500` ms.
    - **max_remote_subscribers** - Maximum possible number of different remote subscribers. Additional remote subscribers will not be acknowledged. The default value is `3`.
    - **find_initial_debounce_reps** - Number of initial debounces using find_initial_debounce_time. This can be used to modify the number of sent messages during initial part of startup (valid values: `0 - 2^8-1`). The default setting is `0`.
    - **find_initial_debounce_time** - Time which the stack collects new service requests before they enter the repetition phase. This can be used to modify the number of sent messages during initial part of startup. The default setting is `200` ms.

<details><summary>Service Discovery configuration</summary>

```
"service-discovery" :
{
    "enable" : "true",
    "multicast" : "239.192.255.251",
    "port" : "30490",
    "protocol" : "udp",
    "initial_delay_min" : "10",
    "initial_delay_max" : "10",
    "repetitions_base_delay" : "30",
    "repetitions_max" : "3",
    "cyclic_offer_delay" : "1000",
    "ttl" : "3"
},
```
</details>


## nPDU Default Timings

This is the add-on documentation for the nPDU feature, aka. **Zugverfahren**.
The nPDU feature can be used to reduce network load as it enables the vsomeip stack to combine multiple vsomeip messages in one single ethernet frame.

Some general important things regarding the nPDU feature first:
- Due to its nature the nPDU feature trades lower network load for speed.
- As the nPDU feature requires some settings which are not transmitted through the service discovery, it's not sufficient anymore to have an json file without a "services" section on the client side.
- As the client- and server-endpoints of a node are managed by the routing manager (which is the application entered at "routing" in the json file) the nPDU feature settings always have to be defined in the json file used by the application acting as routing manager.
- The nPDU feature timings are defined in milliseconds.
- Node internal communication over UNIX domain sockets is not affected by the nPDU feature.
- If the debounce times configuration for a method in the json file is missing or incomplete the default values are used: `2ms debounce time` and `5ms max retention time`. The global default values can be overwritten via the `npdu-default-timings` json object.

There are two parameters specific for the nPDU feature:
- **debounce-time-xxx** - minimal time between sending a message to the same method of a remote service over the same connection (src/dst address + src/dst port).
- **max-retention-xxx** - the maximum time which a message to the same method of a remote service over the same connection (src/dst address + src/dst port) is allowed to be buffered on sender side.

For more information please see the corresponding requirement documents.

The nPDU feature specific settings are configured in the json file in the `"services"` section on service level in a special `debounce-times` section:

<details><summary>Simple Example</summary>

```
"services":
[
    {
        "service":"0x1000",
        "instance":"0x0001",
        "unreliable":"30509",
        "debounce-times":
        {
            // nPDU feature configuration for this
            // service here
        }
    }
],
```
</details>

<details><summary>Global nPDU configurations</summary>
Additionally nPDU default timings can be configured globally.

The global default timings can be overwritten via the `npdu-default-timings` json object. For example the following configuration snippet shows how to set all default timings to zero:

```
    "npdu-default-timings" : {
        "debounce-time-request" : "0",
        "debounce-time-response" : "0",
        "max-retention-time-request" : "0",
        "max-retention-time-response" : "0"
    },
```
</details>

<details><summary>Example 1: One service with one method offered over UDP</summary>

- The service is hosted on IP: 192.168.1.9.
- The service is offered on port 30509 via UDP.
- The service has the ID 0x1000
- The method has the ID 0x0001
- The client accesses the service from IP: 192.168.1.77

**Service side**

Debounce time for responses should have a:
- debounce time of 10 milliseconds
- maximum retention time of 100 milliseconds

```
{
    "unicast":"192.168.1.9",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unreliable":"30509",
            "debounce-times":
            {
                "responses": {
                    "0x1001" : {
                        "debounce-time":"10",
                        "maximum-retention-time":"100"
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```

**Client side**

Debounce time for requests to the service on 192.168.1.9 should have a:
- debounce time of 20 milliseconds
- maximum retention time of 200 milliseconds

```
{
    "unicast":"192.168.1.77",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unicast":"192.168.1.9", // required to mark service as external
            "unreliable":"30509",
            "debounce-times":
            {
                "requests": {
                    "0x1001" : {
                        "debounce-time":"20",
                        "maximum-retention-time":"200"
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```
</details>

<details><summary>Example 2: One service with two methods offered over UDP</summary>

- The service is hosted on IP: 192.168.1.9.
- The service is offered on port 30509 via UDP.
- The service has the ID 0x1000
- The method has the ID 0x0001
- The second method has the ID 0x0002
- The client accesses the service from IP: 192.168.1.77

**Service side**

Debounce time for responses should have a:
- debounce time of 10 milliseconds for method 0x1001 and 20 for 0x1002
- maximum retention time of 100 milliseconds for method 0x1001 and 200 for 0x1002

```
{
    "unicast":"192.168.1.9",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unreliable":"30509",
            "debounce-times":
            {
                "responses": {
                    "0x1001" : {
                        "debounce-time":"10",
                        "maximum-retention-time":"100"
                    },
                    "0x1002" : {
                        "debounce-time":"20",
                        "maximum-retention-time":"200"
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```

**Client side**

Debounce time for requests to the service on 192.168.1.9 should have a:
- debounce time of 20 milliseconds for method 0x1001 and 40 for 0x1002
- maximum retention time of 200 milliseconds for method 0x1001 and 400 for 0x1002

```
{
    "unicast":"192.168.1.77",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unicast":"192.168.1.9", // required to mark service as external
            "unreliable":"30509",
            "debounce-times":
            {
                "requests": {
                    "0x1001" : {
                        "debounce-time":"20",
                        "maximum-retention-time":"200"
                    },
                    "0x1002" : {
                        "debounce-time":"40",
                        "maximum-retention-time":"400"
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```
</details>

<details><summary>Example 3: One service with one method offered over UDP and TCP</summary>

- The service is hosted on IP: 192.168.1.9.
- The service is offered on port 30509 via UDP.
- The service is offered on port 30510 via TCP.
- The service has the ID 0x1000
- The method has the ID 0x0001
- The client accesses the service from IP: 192.168.1.77

**Service side**

Debounce time for responses should have a:
- debounce time of 10 milliseconds
- maximum retention time of 100 milliseconds
- TCP should use the same settings as UDP

```
{
    "unicast":"192.168.1.9",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unreliable":"30509",
            "reliable":
            {
                "port":"30510",
                "enable-magic-cookies":"false"
            },
            "debounce-times":
            {
                "responses": {
                    "0x1001" : {
                        "debounce-time":"10",
                        "maximum-retention-time":"100",
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```

**Client side**

Debounce time for requests to the service on 192.168.1.9 should have a:
- debounce time of 20 milliseconds
- maximum retention time of 200 milliseconds
- TCP should use the same settings as UDP

```
{
    "unicast":"192.168.1.77",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x0001",
            "unicast":"192.168.1.9", // required to mark service as external
            "unreliable":"30509",
            "reliable":
            {
                "port":"30510",
                "enable-magic-cookies":"false"
            },
            "debounce-times":
            {
                "requests": {
                    "0x1001" : {
                        "debounce-time":"20",
                        "maximum-retention-time":"200",
                    }
                }
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```
</details>

## Services

- **services** (array) - Contains the services of the service provider.
    - **service** - The id of the service.
    - **instance** - The id of the service instance.
    - **protocol** (optional) - The protocol that is used to implement the service instance. The default value is `someip`. If a different setting is provided, vsomeip does not open the specified port (server side) or does not connect to the specified port (client side). Thus, this option can be used to let the service discovery announce a service that is externally implemented.
    - **unicast** (optional) - The unicast that hosts the service instance.
    **NOTE**: The unicast address is needed if external service instances shall be used, but service discovery is disabled. In this case, the provided unicast address is used to access the service instance.
    - **reliable** - Specifies that the communication with the service is reliable respectively the TCP protocol is used for communication.
        - **port** - The port of the TCP endpoint.
        - **enable-magic-cookies** - Specifies whether magic cookies are enabled, valid values are `true` or `false`. The default value is `false`.
    - **unreliable** - Specifies that the communication with the service is unreliable respectively the UDP protocol is used for communication (valid values: the port of the UDP endpoint).
    - **events** (array) - Contains the events of the service.
        - **event** - The id of the event.
        - **is_field** - Specifies whether the event is of type field.
        **NOTE**: A field is a combination of getter, setter and notification event. It contains at least a `getter`, a `setter`, or a `notifier`. The notifier sends an event message that transports the current value of a field on change.
        - **is_reliable** - Specifies whether the communication is reliable respectively whether the event is sent with the TCP protocol, valid values are `true` or `false`. If the value is false the UDP protocol will be used.
        - **cycle** - Defines the period for events to be sent. (values are defined in `ms`).
        - **update_on_change** - Defines if the updates are sent right away if the event value changes, valid values are `true` or `false`. The default value is `true`.
        - **change_resets_cycle** - When the `update_on_change` is set to `true`, and this parameter is also `true`, the defined cycle will be reset when the event value changes. Valid values are `true` or `false`. The default value is `false`.
    - **eventgroups** (array) - Events can be grouped together into on event group. For a client it is thus possible to subscribe for an event group and to receive the appropriate events within the group.
        - **eventgroup** - The id of the event group.
        - **multicast** - Specifies the multicast that is used to publish the eventgroup.
            - **address** - The multicast address.
            - **port** - The multicast port.
        - **events** (array) - Contains the ids of the appropriate events.
        - **threshold** - Specifies when to use multicast and when to use unicast to send a notification event. Must be set to a non-negative number. If it is set to zero, all events of the eventgroup will be sent by unicast. Otherwise, the events will be sent by unicast as long as the number of subscribers is lower than the threshold and by multicast if the number of subscribers is greater or equal. This means, a threshold of 1 will lead to all events being sent by multicast. The default value is `0`.
    - **debounce-times** (object) - Used to configure the nPDU feature. This is described in detail in SOME/IP [nPDU Default Timings](#npdu-default-timings).
        - **requests** - Requests configuration
            - **debounce-time** - minimal time between sending a message to the same method of a remote service over the same connection (src/dst address + src/dst port).
            - **maximum-retention-time** - the maximum time which a message to the same method of a remote service over the same connection (src/dst address + src/dst port) is allowed to be buffered on sender side.
        - **responses** - Responses configuration
            - **debounce-time** - minimal time between sending a message to the same method of a remote service over the same connection (src/dst address + src/dst port).
            - **maximum-retention-time** - the maximum time which a message to the same method of a remote service over the same connection (src/dst address + src/dst port) is allowed to be buffered on sender side.
    - **someip-tp** (object) Used to configure the SOME/IP-TP feature. With SOME/IP Transport Protocol (TP) it is possible to transport messages which exceed the UDP payload size limit of 1400 byte. If enabled the message is segmented and send in multiple UDP datagrams.
        - **service-to-client** (array) - Contains the IDs for responses, fields and events which are sent from the node to a remote client which can be segmented via SOME/IP-TP if they exceed the maximum message size for UDP communication. If an ID isn't listed here the message will otherwise be dropped if the maximum message size is exceeded.
            - **method** - configures the method id to use
            - **max-segment-length** - new UDP payload in bytes, value must be a multiple of 16.
            - **separation-time** - lower limit used between sending of two segments of the same SOME/IP-TP message. Default for the separation time is 0, no matter whether a message is SOME/IP-TP or not. For separation time 0, message sending is no different from what it was before.
        - **client-to-service** (array) - Contains the IDs for requests, which are sent from the node to a remote service which can be segmented via SOME/IP-TP if they exceed the maximum message size for UDP communication. If an ID isn't listed here the message will otherwise be dropped if the maximum message size is exceeded. Please note that the unicast key has to be set to the remote IP address of the offering node for this setting to take effect.
            - **method** - configures the method id to use
            - **max-segment-length** - new UDP payload in bytes, value must be a multiple of 16.
            - **separation-time** - lower limit used between sending of two segments of the same SOME/IP-TP message. Default for the separation time is 0, no matter whether a message is SOME/IP-TP or not. For separation time 0, message sending is no different from what it was before.

<details><summary>Services configuration</summary>

```
"services" :
	[
		{
			"service" : "0x0106",
			"instance" : "0x63",
			"unreliable" : "30506",
			"reliable" :
			{
				"port" : "30506",
				"enable-magic-cookies" : "true"
			},
			"someip-tp" : {
				"service-to-client": [ "0x0106", "0x0001"]
			}
		},
        {
			"service" : "0x1002",
			"instance" : "0x63",
			"protocol" : "hsfz",
			"reliable" :
			{
				"port" : "6801",
				"enable-magic-cookies" : "false"
			}
		},
		{
			"service" : "0x0101",
			"instance" : "0x63",
			"unreliable" : "30506",
			"reliable" :
			{
				"port" : "30506",
				"enable-magic-cookies" : "true"
			},
			"events" :
			[
				{
					"event" : "0x8001",
					"is_field" : "false"
				},
				{
					"event" : "0x8003",
					"is_field" : "false",
					"is_reliable" : "true"
				}
			],
			"eventgroups" :
			[
				{
					"eventgroup" : "0x06",
					"multicast" : {"address" : "239.192.255.251", "port" : "30506" },
					"threshold" : "1",
					"events" :
					[
						"0x800B"
					]
				}
			]
		}
	]
```
</details>

<details><summary>SOME/IP TP configuration</summary>

- Service 0x1111/0x1 is hosted on 192.168.0.1 on UDP port 40000
- Client is running on 192.168.0.100
- The service has two methods with ID: 0x1 and 0x2 which require large requests and large responses. Additionally the service offers a field with ID 0x8001 which requires a large payloads as well.
- The maximum payload size on service side should be limited to 5000 bytes.

**Configuration service side:**

```
{
    "unicast":"192.168.0.1",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x1",
            "unreliable":"40000",
            "someip-tp": {
                "service-to-client": [
                    "0x1", "0x2", "0x8001"
                ]
            }
        }
    ],
    "max-payload-size-unreliable" : "5000",
    "routing":"[...]",
    "service-discovery": { [...] }
}
```

**Configuration client side:**
```
{
    "unicast":"192.168.0.100",
    "logging": { [...] },
    "applications": [ [...] ],
    "services":
    [
        {
            "service":"0x1000",
            "instance":"0x1",
            "unreliable":"40000", // required to mark service as external
            "someip-tp": {
                "client-to-service": [
                    "0x1", "0x2"
                ]
            }
        }
    ],
    "routing":"[...]",
    "service-discovery": { [...] }
}
```

</details>

<details><summary>SOME/IP TP other example</summary>

```
"services" :
[
    {
        "service" : "0x1234",
        "instance" : "0x1",
        "unicast" : "192.168.0.105",
        "unreliable" : "34503",
        "someip-tp": {
            "client-to-service": [
                {
                    "method" : "0x12",
                    "max-segment-length" : "1400",
                    "separation-time" : "20"
                }
            ]
        }
    }
],

```
</details>



## Internal Services

- **internal_services** (optional array) - Specifies service/instance ranges for pure internal service-instances. This information is used by vsomeip to avoid sending Find-Service messages via the Service-Discovery when a client is requesting a not available service-instance. Its can either be done on service/instance level or on service level only which then includes all instance from 0x0000-0xffff.
    - **first** - The lowest entry of the internal service range.
        - **service** - The lowest Service-ID in hex of the internal service range.
        - **instance** (optional) - The lowest Instance-ID in hex of a internal service-instance range. If not specified the lowest Instance-ID is 0x0000.
    - **last** - The highest entry of the internal service range.
        - **service** - The highest Service-ID in hex of a internal service range.
        - **instance** (optional) - The highest Instance-ID in hex of a internal service-instance range. If not specified the highest Instance-ID is 0xFFFF.

<details><summary>Internal Services configuration</summary>

```
"internal_services" :
    [
        {
            "first" : "0xF700",
            "last" : "0xFEFF"
        },
        {
            "first" : "0xFF40",
            "last" : "0xFF5F"
        },
        {
            "first" :
            {
                "service" : "0xB04C",
                "instance" : "0x1"
            },
            "last" :
            {
                "service" : "0xB04C",
                "instance" : "0x1"
            }
        }
    ],
```

</details>

## Clients

- **clients** (array) - The client-side ports that shall be used to connect to a specific service. For each service, an array of ports to be used for reliable/unreliable communication can be specified. vsomeip will take the first free port of the list. If no free port can be found, the connection will fail. If vsomeip is asked to connect to a service instance without specified port(s), the port will be selected by the system. This implies that the user has to ensure that the ports configured here do not overlap with the ports automatically selected by the IP stack.
    - **service** - Specify the service the port configuration shall be applied to.
    - **instance** - Specify the instance the port configuration shall be applied to.
    - **reliable** (array) - The list of client ports to be used for reliable (TCP) communication to the given service instance. Ports can be specified in dec or hex format.
    - **unreliable** (array) - The list of client ports to be used for unreliable (UDP) communication to the given service instance. Ports can be specified in dec or hex format.

        Additionally there is the possibility to configure mappings between ranges of client ports and ranges of remote service ports. (If a client port is configured for a specific service/instance, the port range mapping is ignored). Using the first/last to specify the lower/upper bound of a port range.

    - **reliable_remote_ports** - Specifies a range of reliable remote service ports
    - **unreliable_remote_ports** - Specifies a range of unreliable remote service ports
    - **reliable_client_ports** - Specifies the range of reliable client ports to be mapped to the reliable_remote_ports range
    - **unreliable_client_ports** - Specifies the range of unreliable client ports to be mapped to the unreliable_remote_ports range

<details><summary>Simple Clients configuration</summary>

```
"clients" :
[
    {
        "service" : "0x1234",
        "instance" : "0x5678",
        "unreliable" : [ 40000, 40002 ]
    }
],
```
</details>

<details><summary>Port mapping clients configuration</summary>

```
"clients" :
[
    {
        "reliable_remote_ports"   : { "first" : "30501", "last" : "30599" },
        "unreliable_remote_ports" : { "first" : "30501", "last" : "30599" },
        "reliable_client_ports"   : { "first" : "30491", "last" : "30499" },
        "unreliable_client_ports" : { "first" : "30491", "last" : "30499" }
    },
    {
        "reliable_remote_ports"   : { "first" : "30501", "last" : "30599" },
        "unreliable_remote_ports" : { "first" : "30501", "last" : "30599" },
        "reliable_client_ports"   : { "first" : "30898", "last" : "30998" },
        "unreliable_client_ports" : { "first" : "30898", "last" : "30998" }
    },
],
```
</details>

## Watchdog

- **watchdog** (optional) - The Watchdog sends periodically pings to all known local clients. If a client isn't responding within a configured time/amount of pongs the watchdog deregisters this application/client. If not configured the watchdog isn't activated.
    - **enable** - Specifies whether the watchdog is enabled or disabled, valid values are `true`, `false`. The default value is `false`.
    - **timeout** - Specifies the timeout in ms the watchdog gets activated if a ping isn't answered with a pong by a local client within that time. (valid values: 2 - 2^32). The default value is `5000` ms.
    - **allowed_missing_pongs** - Specifies the amount of allowed missing pongs. (valid values: 1 - 2^32). The default value is `3`.

## Local Clients Keepalive

- **local-clients-keepalive** (optional) - The Local Clients Keepalive option activates the sending of periodic ping messages from the routing manager clients to the routing host. The routing manager host shall reply to the ping with a pong. The idea is to have a simpler alternetive to the TCP_KEEPALIVE, particularly for systems where this option can not be configured.
    - **enable** - Specifies whether the Local Clients Keepalive is enabled or disabled, valid values are `true`, `false`. The default value is `false`.
    - **time** - Specifies the time in ms the Local Clients Keepalive messages are sent. The default value is `5000` ms.

## Selective Broadcasts Support

- **supports_selective_broadcasts** (optional array) - This nodes allow to add a list of IP addresses on which CAPI-Selective-Broadcasts feature is supported. If not specified the feature can't be used and the subscription behavior of the stack is same as with normal events.
    - **address** - Specifies an IP-Address (in IPv4 or IPv6 notation) on which the "selective"-feature is supported. Multiple addresses can be configured.

<details><summary>Selective Broadcasts Support configuration</summary>

```
"supports_selective_broadcasts" :
{
    "address" : "10.10.3.10"
}
```
</details>

## E2E

- **e2e** - Used to configure the E2E protection for the specified events
    - **e2e_enabled** - Specifies if E2E protection should be enabled or disabled. Use `true` to enable.
    - **protected** - Specify the protected events
        - **service_id** - Specifies the service ID.
        - **event_id** - Specifies the event ID to be protected.
        - **variant** -  Specifies if it should protect (send) or check (receive) the messages, or both. Possible values are `protector`, `checker` and `both`, respectively.
        - **profile** - Specify the E2E profile to be used. Valid values are `CRC8` for Profile 01, `P04` for Profile 04, `P07` for Profile 07 and `CRC32` for the Custom Profile with ethernet CRC.
            - Profile 01
                - **data_id** - Specifies a system wide unique 16 bit numerical identifier.
                - **crc_offset** - Specifies the offset of the CRC in bytes.
                - **data_length** - Specifies the length of all data in bits.
                - **counter_offset** - Specifies the offset of the counter in bits. Default value is `8`.
                - **data_id_nibble_offset** - Specifies the offset of the dataID nibble. Default value is `12`.
                - **data_id_mode** - Specifies the dataID mode (valid values are `0`, `1`, `2` and `3`). It impacts which part of the dataID is used in CRC calculation and if any port of the dataID is sent in the E2E Header.
            - Profile 04
                - **data_id** - Specifies a system wide unique 32 bit numerical identifier.
                - **crc_offset** - Specifies the offset of the E2E header in bits. It must start at 64 (8 bits) and be a multiple of 8.
                - **min_data_length** - Specifies the minimum length of the data in bits. Default value is `0`.
                - **max_data_length** - Specifies the maximum length of the data in bits. Default value is `0xFFFF`.
                - **max_delta_counter** - Specifies the maximum allowed difference between the counter value of the current message and the previous valid message. Default value is `0xFFFF`.
            - Profile 07
                - **data_id** - Specifies a system wide unique 32 bit numerical identifier.
                - **crc_offset** - Specifies the offset of the E2E header in bits. It must start at 64 (8 bits) and be a multiple of 8.
                - **min_data_length** - Specifies the minimum length of the data in bits. Default value is `0`.
                - **max_data_length** - Specifies the maximum length of the data in bits. Default value is `0xFFFFFFFF`.
                - **max_delta_counter** - Specifies the maximum allowed difference between the counter value of the current message and the previous valid message. Default value is `0xFFFFFFFF`.
            - Custom
                - **crc_offset** - Specifies the offset of the CRC (same as E2E Header in this profile) in bytes.

<details><summary>Simple E2E configuration</summary>

```
"e2e" :
    {
        "e2e_enabled" : "true",
        "protected" :
        [
            {
                "service_id" : "0x1507",
                "event_id" : "0x01",
                "profile" : "CRC32",
                "variant" : "protector",
                "crc_offset" : "0"
            },
            {
                "service_id" : "0x1507",
                "event_id" : "0x8001",
                "profile" : "CRC8",
                "variant" : "checker",
                "crc_offset" : "0",
                "data_id_mode" : "3",
                "data_length" : "56",
                "data_id" : "0xA73"
            },
            {
                "service_id" : "0x1507",
                "event_id" : "0x02",
                "profile" : "CRC8",
                "variant" : "checker",
                "crc_offset" : "0",
                "data_id_mode" : "3",
                "data_length" : "56",
                "data_id" : "0xA73"
            }
        ]
    },
```
</details>

<details><summary>E2E Profile 01 configuration</summary>

**Server-side**

```
"e2e" :
{
    "e2e_enabled" : "true",
    "protected" :
    [
        {
            "service_id" : "0x1234",
            "event_id" : "0x80e9",
            "profile" : "CRC8",
            "variant" : "protector",
            "crc_offset" : "0",
            "data_id_mode" : "3",
            "data_length" : "24",
            "data_id" : "0x1FF"
        },
        {
            "service_id" : "0x1234",
            "event_id" : "0x80ea",
            "profile" : "CRC8",
            "variant" : "protector",
            "crc_offset" : "0",
            "data_id_mode" : "3",
            "data_length" : "24",
            "data_id" : "0x1FF"
        }
    ]
},
```

**Client-side**

```
"e2e" :
{
    "e2e_enabled" : "true",
    "protected" :
    [
        {
            "service_id" : "0x1234",
            "event_id" : "0x80e9",
            "profile" : "CRC8",
            "variant" : "checker",
            "crc_offset" : "0",
            "data_id_mode" : "3",
            "data_length" : "24",
            "data_id" : "0x1FF"
        },
        {
            "service_id" : "0x1234",
            "event_id" : "0x80ea",
            "profile" : "CRC8",
            "variant" : "checker",
            "crc_offset" : "0",
            "data_id_mode" : "3",
            "data_length" : "24",
            "data_id" : "0x000"
        }
    ]
},
```

</details>

<details><summary>E2E Profile 04 configuration</summary>

**Server-side**

```
"e2e" :
{
    "e2e_enabled" : "true",
    "protected" :
    [
        {
            "service_id" : "0x1234",
            "event_id" : "0x80e9",
            "profile" : "P04",
            "variant" : "protector",
            "crc_offset" : "64",
            "data_id" : "0xFF"
        },
        {
            "service_id" : "0x1234",
            "event_id" : "0x80ea",
            "profile" : "P04",
            "variant" : "protector",
            "crc_offset" : "64",
            "data_id" : "0xFF"
        }
    ]
},
```

**Client-side**

```
"e2e" :
{
    "e2e_enabled" : "true",
    "protected" :
    [
        {
            "service_id" : "0x1234",
            "event_id" : "0x80e9",
            "profile" : "P04",
            "variant" : "checker",
            "crc_offset" : "64",
            "data_id" : "0xFF"
        },
        {
            "service_id" : "0x1234",
            "event_id" : "0x80ea",
            "profile" : "P04",
            "variant" : "checker",
            "crc_offset" : "64",
            "data_id" : "0x00"
        }
    ]
},
```

</details>


## Debounce

- **debounce** (optional array) - Events/fields sent by external devices will be forwarded to the applications only if a configurable function evaluates to true. The function checks whether the event/field payload has changed and whether a specified interval has been elapsed since the last forwarding.
    - **service** - Service ID which hosts the events to be debounced.
    - **instance** - Instance ID which hosts the events to be debounced.
    - **events** - Array of events which shall be debounced based on the following configuration options.
        - **event** - Event ID.
        - **on_change** - Specifies whether the event is forwarded on payload change or not, valid values: `true`, `false`. The default value is `false`.
        - **ignore** - Array of payload indexes with given bit mask (optional) to be ignored in payload change evaluation. Instead of specifying an index / bitmask pair, one can only define the payload index which shall be ignored in the evaluation.
            - **index** - Payload index to be checked with given bitmask.
            - **mask** - 1 Byte bitmask applied to byte at given payload index. **Example mask**: 0x0f ignores payload changes in low nibble of the byte at given index.
        - **interval** - Specifies if the event shall be debounced based on elapsed time interval. (valid values: `time in ms`, `never`). The default value is  `never`.
        - **on_change_resets_interval** (optional) - Specifies if interval timer is reset when payload change was detected, valid values are `false`, `true`. The default value is `false`.
        - **send_current_value_after** (optional) - Specifies if last message should be sent after interval timeout, valid values are `false`, `true`. The default value is `false`.


<details><summary>Debounce configuration</summary>

```
"debounce" :
[
    {
        "service" : "0x7530",
        "instance" : "0x0001",
        "events" :
        [
            {
                "event" : "0x8001",
                "on_change" : "false",
                "interval" : "50"
            },
            {
                "event" : "0x8002",
                "on_change" : "false",
                "interval" : "50"
            },
            {
                "event" : "0x8005",
                "on_change" : "false",
                "interval" : "50"
            },
            {
                "event" : "0x8006",
                "on_change" : "false",
                "interval" : "100"
            },
            {
                "event" : "0x8009",
                "on_change" : "false",
                "interval" : "100"
            }
        ]
    }
]
```
</details>


## Acceptances

- **acceptances** - Can be used to modify the assignment of ports to the unsecure, optional and secure ranges.
    - **address** - The IP Address of the device where the ports should be modified.
    - **path** <`TBD`> -
    - **reliable** - Type of ports to modify. Possible values are `reliable` and `unreliable`.
        - **port** - Adds that port to the secure port group.
        - **first** -  Group of ports starting at `first`, to add as a secure port.
        - **last** -  Group of ports ending at `last`, to add as a secure port.
        - **type** - Used to specify the type of ports to remove from secure ports group. Possible values are `optional` and `secure`.
            - `optional`, removes the following ports from the secure port range:
                - Closed(30491, 30499)
                - Closed(30898, 30998)
                - Closed(30501, 30599)
            - `secure`, removes the following ports from the secure port range:
                - Closed(32491, 32499)
                - Closed(32898, 32998)
                - Closed(32501, 32599)

<details><summary>Acceptances configuration</summary>
Configuration used to specify the port group 30xy from the IP 10.3.0.10 should be treated as "never-secure".

```
"acceptances" :
[
    {
    "address" : "10.3.0.10",
        "unreliable" :
        [
            {
                "type" : "optional"
            }
        ],
        "reliable" :
        [
            {
                "type" : "optional"
            }
        ]
    }
]
```
</details>

## Secure Services

- **secure-services** - List of service instances that are only accepted, if being offered on a secure port.
    - **service** - The id of the service
    - **instance** - The id of the instance


## Partitions

- **partitions** - Allows to group service instances that are offered on the same port into partitions. For each partition, a separate client port will be used. The goal is to enable faster processing of specific
events if a single server port is used to offer many services that send many messages, especially at startup.
    - **service** - The id of the service
    - **instance** - The id of the instance

<details><summary>Partitions configuration</summary>

```
"partitions" :
[
    [
        {
            "service": "0x100b",
            "instance" : "0x0001"
        },
        {
            "service" : "0x3056",
            "instance" : "0x0001"
        },
        {
            "service" : "0x3059",
            "instance" : "0x0001"
        },
        {
            "service" : "0x3077",
            "instance" : "0x0001"
        }
    ]
],
```
</details>


## Suppress Events

- **suppress_missing_event_logs**  - Used to filter the log message `deliver_notification: Event [1234.5678.80f3]
is not registered. The message is dropped.` that occurs whenever vSomeIP
receives an event without having a corresponding object being registered.
    - **service** - Service ID of event to be filtered. Possible values: hex, dec, any.
    - **instance** - Instance ID of event to be filtered. Possible values: hex, dec, any.
    - **events** - Array of events to be filtered. **Possible values**:
        - Single hex, dec value.
        - Multiple hex, dec values.
        - Range based hex, dec values.
        - If no events is provided, then it's assumed all events are to be ignored.


<details><summary>Suppress Events configuration</summary>

```
"suppress_missing_event_logs" :
[
    {
        "service" : "0x0023",
        "instance" : "0x0001",
        "events" : [ "0x8001", "0x8002",
                    {
                        "first" : "0x8010",
                        "last" : "0x801f"
                    },
                    "0x8020" ]
    },
    {
        "service" : "0x0023",
        "instance" : "0x0002",
        "events" : [ "0x8005", "0x8006" ]
    },
    {
        "service" : "0x0023",
        "instance" : "0x0002",
        "events" : [ "0x8105", "0x8106" ]
    },
    {
        // no "events" --> ignore all(!) events of these services
        "service" : "any",
        "instance" : "0x00f2"
    },
    {
        "service" : "0x0102",
        "instance" : "any",
        "events" : [ "0x8005", "0x8006" ]
    }
]
```
</details>


# Environment Variables

On startup of a vSomeIP application, the following environment variables are read:

- **VSOMEIP_APPLICATION_NAME**: This environment variable is used to specify the name of the application. This name is later used to map a client id to the application in the configuration file. It is independent from the application's binary name.

- **VSOMEIP_CONFIGURATION**: vsomeip uses the default configuration file `/etc/vsomeip.json` and/or the default configuration folder `/etc/vsomeip`. This can be overridden by a local configuration file `./vsomeip.json` and/or a local configuration folder `./vsomeip`.

    If VSOMEIP_CONFIGURATION is set to a valid file or directory path, this is used instead of the standard configuration (thus neither default nor local file/folder will be parsed).

    **NOTES**:
    - If the file/folder that is configured by VSOMEIP_CONFIGURATION does not exist, the default configuration locations will be used.

    - vsomeip will parse and use the configuration from all files in a configuration folder but will not consider directories within the configuration folder.

- **VSOMEIP_CONFIGURATION_\<application\>**: Application-specific version of `VSOMEIP_CONFIGURATION`. Please note that must be valid as part of an environment variable.

- **VSOMEIP_MANDATORY_CONFIGURATION_FILES**: vsomeip allows to specify mandatory configuration files to speed-up application startup. While mandatory configuration files are read by all applications, all other configuration files are only read by the application that is responsible for connections to external devices.

    If this configuration variable is not set, the default mandatory files `vsomeip_std.json`, `vsomeip_app.json`, `vsomeip_events.json`, `vsomeip_plc.json`, `vsomeip_log.json`, `vsomeip_security.json`, `vsomeip_whitelist.json`, `vsomeip_policy_extensions.json`, `vsomeip_portcfg.json` and `vsomeip_device.json` are used.


- **VSOMEIP_CLIENTSIDELOGGING**: Set this variable to an empty string to enable logging of any received messages to DLT in all applications acting as routing manager proxies. For example add the following line to the applications systemd service file:
Environment=VSOMEIP_CLIENTSIDELOGGING=""
To enable service-specific logs, provide a space- or colon-separated list of ServiceIDs (using 4-digit hexadecimal notation, optionally followed by dot-separated InstanceID). For example:

        Environment=VSOMEIP_CLIENTSIDELOGGING="b003.0001 f013.000a 1001 1002"
        Environment=VSOMEIP_CLIENTSIDELOGGING="b003.0001:f013.000a:1001:1002"


- **VSOMEIP_CONFIGURATION_MODULE** - <`TBD`>

- **VSOMEIP_E2E_PROTECTION_MODULE** - <`TBD`>

- **VSOMEIP_LOAD_PLUGINS** - <`TBD`>

<details><summary>Using environment variables</summary>

In the following example the application `my_vsomeip_application` is started.

The settings are read from the file `my_settings.json` in the current working directory.

The client id for the application can be found under the name
`my_vsomeip_client` in the configuration file.

```bash
#!/bin/bash
export VSOMEIP_APPLICATION_NAME=my_vsomeip_client
export VSOMEIP_CONFIGURATION=my_settings.json
./my_vsomeip_application
```
</details>

---
[Back to top](#)
