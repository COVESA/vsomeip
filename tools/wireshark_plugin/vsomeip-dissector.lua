--[[
    vsomeip-dissector.lua V0.3
    Wireshark Lua vsomeip protocol dissector
    author: rui.graca@ctw.bmwgroup.com
--]]

protocol_name = 'vsomeip3'
config = 'vsomeip_config'

vsomeip_protocol = Proto(protocol_name, protocol_name:upper() .. ' Protocol')
vsomeip_command = ProtoField.uint8(protocol_name .. '.command' , "Command", base.HEX)
vsomeip_version = ProtoField.uint16(protocol_name .. '.version' , "Version", base.DEC)
vsomeip_client = ProtoField.uint16(protocol_name .. '.client' , "Client", base.HEX)
vsomeip_size = ProtoField.uint32(protocol_name .. '.size' , "Size", base.DEC)
vsomeip_newclient = ProtoField.uint16(protocol_name .. '.newclient' , "Assigned Client", base.HEX)

vsomeip_name = ProtoField.string(protocol_name .. '.name' , "Name")

vsomeip_instance = ProtoField.uint16(protocol_name .. '.instance' , "Instance", base.HEX)
vsomeip_reliable = ProtoField.uint8(protocol_name .. '.reliable' , "Reliable", base.DEC)
vsomeip_crc = ProtoField.bytes(protocol_name .. '.crc' , "CRC", base_HEX)
vsomeip_dstClient = ProtoField.uint16(protocol_name .. '.dstClient' , "Dst Client", base.HEX)
vsomeip_payload = ProtoField.bytes(protocol_name .. '.payload' , "Payload", base_HEX)

vsomeip_service = ProtoField.uint16(protocol_name .. '.service' , "Service", base.HEX)
vsomeip_eventgroup = ProtoField.uint16(protocol_name .. '.eventgroup' , "Eventgroup", base.HEX)
vsomeip_subscriber = ProtoField.uint16(protocol_name .. '.subscriber' , "Subscriber", base.HEX)
vsomeip_notifier = ProtoField.uint16(protocol_name .. '.notifier' , "Notifier", base.HEX)
vsomeip_event = ProtoField.uint16(protocol_name .. '.event' , "Event", base.HEX)
vsomeip_id = ProtoField.uint16(protocol_name .. '.id' , "ID", base.HEX)
vsomeip_port = ProtoField.uint16(protocol_name .. '.port' , "Port", base.DEC)

vsomeip_provided = ProtoField.uint8(protocol_name .. '.provided' , "Provided", base.DEC)

vsomeip_major = ProtoField.uint8(protocol_name .. '.major' , "Major", base.HEX)
vsomeip_minor = ProtoField.uint32(protocol_name .. '.minor' , "Minor", base.HEX)

vsomeip_offer_type = ProtoField.uint8(protocol_name .. '.offer_type' , "OfferType", base.DEC)

vsomeip_pending_offer_id = ProtoField.uint32(protocol_name .. '.pending_offer_id' , "PendingOfferId", base.HEX)

vsomeip_update_id = ProtoField.uint32(protocol_name .. '.update_id' , "UpdateId", base.HEX)
vsomeip_uid = ProtoField.uint32(protocol_name .. '.uid' , "UID", base.HEX)
vsomeip_gid = ProtoField.uint32(protocol_name .. '.gid' , "GID", base.HEX)
vsomeip_policy_count = ProtoField.uint32(protocol_name .. '.policy_count' , "PoliciesCount", base.HEX)
vsomeip_policy = ProtoField.bytes(protocol_name .. '.policy' , "Policy", base_HEX)

vsomeip_filter = ProtoField.bytes(protocol_name .. '.filter' , "Filter", base_HEX)

vsomeip_offered_services = ProtoField.bytes(protocol_name .. '.offered_services' , "OfferedServices", base_HEX)

vsomeip_configurations_key_size = ProtoField.uint32(protocol_name .. '.key_size' , "Key Size", base.DEC)
vsomeip_configurations_key = ProtoField.string(protocol_name .. '.key' , "Key")
vsomeip_configurations_value_size = ProtoField.uint32(protocol_name .. '.value_size' , "Value Size", base.DEC)
vsomeip_configurations_value = ProtoField.string(protocol_name .. '.value' , "Value")

vsomeip_credentials = ProtoField.bytes(protocol_name .. '.credentials' , "Credentials", base_HEX)
vsomeip_policies = ProtoField.bytes(protocol_name .. '.policies' , "Policies", base_HEX)

vsomeip_protocol.fields = {
    vsomeip_command, vsomeip_version,
    vsomeip_client, vsomeip_size,
    vsomeip_name, vsomeip_newclient,
    vsomeip_instance, vsomeip_reliable, vsomeip_crc, vsomeip_dstClient, vsomeip_payload,
    vsomeip_service, vsomeip_eventgroup, vsomeip_subscriber, vsomeip_notifier, vsomeip_event, vsomeip_id,
    vsomeip_port,
    vsomeip_provided,
    vsomeip_major, vsomeip_minor,
    vsomeip_filter,
    vsomeip_configurations_key_size,
    vsomeip_configurations_key,
    vsomeip_configurations_value_size,
    vsomeip_configurations_value
}

-- VSOMEIP_ROUTING_INFO
vsomeip_routing_info_name = 'vsomeip3_routing_info'
vsomeip_routing_info = Proto(vsomeip_routing_info_name, vsomeip_routing_info_name:upper())
vsomeip_routing_info_command = ProtoField.uint8(vsomeip_routing_info_name .. '.subcommand' , "SubCommand", base.HEX)
vsomeip_routing_info_size = ProtoField.uint32(vsomeip_routing_info_name .. '.size' , "Size", base.DEC)
vsomeip_routing_info_client = ProtoField.uint16(vsomeip_routing_info_name .. '.client' , "Client", base.HEX)
vsomeip_routing_info_address = ProtoField.ipv4(vsomeip_routing_info_name .. '.address' , "Address", base_HEX)
vsomeip_routing_info_port = ProtoField.uint16(vsomeip_routing_info_name .. '.port' , "Port", base.DEC)
vsomeip_routing_info_size_client = ProtoField.uint32(vsomeip_routing_info_name .. '.sizeClient' , "Client Info Size", base.DEC)
vsomeip_routing_info_size_service = ProtoField.uint32(vsomeip_routing_info_name .. '.sizeService' , "Service Info Size", base.DEC)

vsomeip_routing_info.fields = {
    vsomeip_routing_info_command,
    vsomeip_routing_info_size,
    vsomeip_routing_info_client,
    vsomeip_routing_info_address,
    vsomeip_routing_info_port,
    vsomeip_routing_info_size_client,
    vsomeip_routing_info_size_service
}

vsomeip_routing_info_service_name = 'vsomeip3_routing_info_service'
vsomeip_routing_info_service = Proto(vsomeip_routing_info_service_name, vsomeip_routing_info_service_name:upper())
vsomeip_routing_info_service_id = ProtoField.uint16(vsomeip_routing_info_service_name .. '.serviceId' , "Service", base.HEX)
vsomeip_routing_info_instance = ProtoField.uint16(vsomeip_routing_info_service_name .. '.instance' , "Instance", base.HEX)
vsomeip_routing_info_major = ProtoField.uint8(vsomeip_routing_info_service_name .. '.major' , "Major", base.DEC)
vsomeip_routing_info_minor = ProtoField.uint32(vsomeip_routing_info_service_name .. '.minor' , "Minor", base.DEC)
vsomeip_routing_info_service.fields = {
    vsomeip_routing_info_service_id,
    vsomeip_routing_info_instance,
    vsomeip_routing_info_major,
    vsomeip_routing_info_minor
}

-- VSOMEIP_REGISTER_EVENT
vsomeip_register_event_name = 'vsomeip3_register_event'
vsomeip_register_event = Proto(vsomeip_register_event_name, vsomeip_register_event_name:upper())
vsomeip_register_event_service = ProtoField.uint16(vsomeip_register_event_name .. '.service', "Service", base.HEX)
vsomeip_register_event_instance = ProtoField.uint16(vsomeip_register_event_name .. '.instance', "Instance", base.HEX)
vsomeip_register_event_notifier = ProtoField.uint16(vsomeip_register_event_name .. '.notifyer', "Notifier", base.HEX)
vsomeip_register_event_type = ProtoField.uint8(vsomeip_register_event_name .. '.type', "Type", base.HEX)
vsomeip_register_event_provided = ProtoField.uint8(vsomeip_register_event_name .. '.provided', "Provided", base.HEX)
vsomeip_register_event_reliability = ProtoField.uint8(vsomeip_register_event_name .. '.reliability', "Reliability", base.HEX)
vsomeip_register_event_iscyclic = ProtoField.uint8(vsomeip_register_event_name .. '.iscyclic', "IsCyclic", base.HEX)
vsomeip_register_event_numeventgroups = ProtoField.uint16(vsomeip_register_event_name .. '.numeventgroups', "Num Eventgroups", base.HEX)

vsomeip_register_event.fields = {vsomeip_register_event_service, vsomeip_register_event_instance,
                                 vsomeip_register_event_notifier, vsomeip_register_event_type,
                                 vsomeip_register_event_provided, vsomeip_register_event_reliability,
                                 vsomeip_register_event_iscyclic, vsomeip_register_event_numeventgroups}

vsomeip_register_event_eventgroup = ProtoField.uint16(vsomeip_register_event_name .. '.eventgroup', "EventGroup", base.HEX)
vsomeip_register_event_eventgroups_name = 'vsomeip3_register_event_eventgroups'
vsomeip_register_event_eventgroups = Proto(vsomeip_register_event_eventgroups_name, vsomeip_register_event_eventgroups_name:upper())
vsomeip_register_event_eventgroups.fields = {vsomeip_register_event_eventgroup}

-- Value to String functions
function get_routing_info_command_name(cmd)
    local cmd_name = "RIE_UNKNOWN"
    if cmd == 0 then cmd_name = "RIE_ADD_CLIENT"
    elseif cmd == 1 then cmd_name = "RIE_DEL_CLIENT"
    elseif cmd == 2 then cmd_name = "RIE_ADD_SERVICE_INSTANCE"
    elseif cmd == 4 then cmd_name = "RIE_DEL_SERVICE_INSTANCE"
    end

  return cmd_name
end

function get_register_event_type_name(cmd)
    local cmd_name = "ET_UNKNOWN"
    if cmd == 0 then cmd_name = "ET_EVENT"
    elseif cmd == 1 then cmd_name = "ET_SELECTIVE_EVENT"
    elseif cmd == 2 then cmd_name = "ET_FIELD"
    end

    return cmd_name
end

function get_reliability(reliable)
    if reliable == 0 then
        return "UDP"
    else
        return "TCP"
    end
end

function get_offer_type(offer)
    if offer == 0 then
        return "LOCAL"
    elseif offer == 1 then
        return "REMOTE"
    elseif offer == 2 then
        return "ALL"
    else
        return "UNKNOWN"
    end
end

-- Helper header reading functions
function try_seek(buffer, offset)
    local new_buffer_size = buffer:len() - offset
    if new_buffer_size <= 0 then
        return nil
    end
    return buffer(offset, new_buffer_size)
end

function read_service_instance_info(buffer, initial_position)
    -- Service Instance Info fields
    local service_id_field_size = 2
    local instance_id_field_size = 2
    local service_id = buffer(initial_position, service_id_field_size)
    local next_position = initial_position + service_id_field_size
    local instance_id = buffer(next_position, instance_id_field_size)
    next_position = next_position + instance_id_field_size
    return service_id, instance_id, next_position
end

function read_someip_major_version(buffer, initial_position)
    local major_version_field_size = 1
    return buffer(initial_position, major_version_field_size), initial_position + major_version_field_size
end

function read_service_info(buffer, initial_position)
    local service_info = {}
    -- Service Info fields
    local next_position = initial_position
    service_info.service_id, service_info.instance_id, next_position = read_service_instance_info(buffer, initial_position)
    service_info.major_version, next_position = read_someip_major_version(buffer, next_position)
    local minor_version_field_size = 4
    service_info.minor_version = buffer(next_position, minor_version_field_size)
    next_position = next_position + minor_version_field_size
    return service_info, next_position
end

function is_routing_service_instance_command(routing_command_name)
    return routing_command_name == "RIE_ADD_SERVICE_INSTANCE" or routing_command_name == "RIE_DEL_SERVICE_INSTANCE"
end

function load_routing_info_client(client_buffer, routing_tree)
    -- Load client as the following:
    ---- Client ID
    local client_id = client_buffer(0, 2)
    routing_tree:add_le(vsomeip_routing_info_client, client_id)

    -- Adds the following if == 8
    ---- IP Address
    ---- Port
    if client_buffer:len() == 8 then
        local client_address = client_buffer(2, 4)
        local client_port = client_buffer(6, 2)
        routing_tree:add(vsomeip_routing_info_address, client_address)
        routing_tree:add_le(vsomeip_routing_info_port, client_port)
    end
end

function load_routing_service_instance_command(buffer, initial_position, routing_tree)
    -- RIE_ADD_SERVICE_INSTANCE and RIE_DEL_SERVICE_INSTANCE subcommands
    local client_size_field_size = 4
    local client_size = buffer(initial_position, client_size_field_size)

    local next_position = initial_position + client_size_field_size
    local client_buffer = buffer(next_position, client_size:le_uint())
    next_position = next_position + client_size:le_uint()
    local services_info_size_field_size = 4
    local services_info_size = buffer(next_position, services_info_size_field_size)
    next_position = next_position + services_info_size_field_size
    local number_of_services = services_info_size:le_uint() / 9

    routing_tree:add_le(vsomeip_routing_info_size_client, client_size)

    load_routing_info_client(client_buffer, routing_tree)

    routing_tree:add_le(vsomeip_routing_info_size_service, services_info_size)
    if number_of_services > 0 then
        routing_info_tree_service = routing_tree:add("Services")
        for i = 0, number_of_services - 1,1 do
            local service_info, new_cursor = read_service_info(buffer, next_position)
            next_position = new_cursor
            routing_info_tree_service_tree = routing_info_tree_service:add(i .. ": [" .. service_info.service_id(1,1)..service_info.service_id(0,1) ..".".. service_info.instance_id(1,1)..service_info.instance_id(0,1) .. "]")
            routing_info_tree_service_tree:add_le(vsomeip_routing_info_service_id, service_info.service_id)
            routing_info_tree_service_tree:add_le(vsomeip_routing_info_instance, service_info.instance_id)
            routing_info_tree_service_tree:add_le(vsomeip_routing_info_major, service_info.major_version)
            routing_info_tree_service_tree:add_le(vsomeip_routing_info_minor, service_info.minor_version)
        end
    end

    return next_position

end

function load_subscribe_filter(buffer, initial_position, filter_field_size, routing_tree)
    -- This field corresponds to a debounce filter (struct debounce_filter_t)
    -- and it can be not present in the message
    -- The parsed message should show something like:
    -- Filter
    --      OnChange                xx
    --      OnChangeResetsInterval  xx
    --      Interval                xx xx xx xx xx xx xx xx
    --      Ignore [
    --          Key: Value          xx xx xx xx xx xx xx xx: xx
    --      ]

    local current_position = initial_position

    local on_change_field_size = 1
    local on_change = buffer(current_position, client_size_field_size)
    current_position = current_position + client_size_field_size

    local on_change_reset_interval_field_size = 1
    local on_change_reset_interval = buffer(current_position, on_change_reset_interval_field_size)
    current_position = current_position + on_change_reset_interval_filed_size

    local interval_field_size = 8
    local on_change_reset_interval = buffer(current_position, interval_field_size)
    current_position = current_position + on_change_reset_interval_filed_size

    local ignore_field_size = filter_field_size - (on_change_field_size + on_change_reset_interval_field_size + interval_field_size)

    local ignore_idx = 0
    local ignore_map = {}
    while ignore_field_size > 0 do
        local key_ignore_field_size = 8
        local key = buffer(current_position, key_field_size)
        current_position = current_position + key_field_size

        local value_field_size = 1
        ignore_map[key] = buffer(current_position, value_field_size)
        current_position = current_position + value_field_size

        ignore_field_size = ignore_field_size - (key_field_size + value_field_size)
    end

    filter_tree = routing_tree:add(vsomeip_filter)

    filter_tree:add("OnChange", on_change)
    filter_tree:add("OnChangeResetsInterval", on_change_reset_interval)
    filter_tree:add("Interval", on_change_reset_interval)
    if filter_field_size > 0 then
        local ignore_tree = filter_tree:add("Ignore")
        for key, value in pairs(ignore_map) do
            ignore_tree:add(key, value)
        end
    end

end

-- Constants
command_field_size = 1
version_field_size = 2
client_field_size = 2
size_field_size = 4
magic_cookie_size = 4
pre_header_size = magic_cookie_size + command_field_size + version_field_size + client_field_size + size_field_size
post_header_size = magic_cookie_size
vsomeip_extra_header_sized = pre_header_size + post_header_size

-- Command handling functions

-- Supports VSOMEIP_ASSIGN_CLIENT structure with:
--  - String with Client Name (variable length)
function assign_client_command(buffer, packet_info, vsomeip_tree)
    local vsomeip_client_name = buffer(0, buffer:len())
    vsomeip_tree:add(vsomeip_name, vsomeip_client_name)
    packet_info.cols.info:append(" "..vsomeip_client_name:string())
end

-- Supports VSOME_ASSIGN_CLIENT_ACK structure with:
--  - Client ID (2 bytes)
function assign_client_ack_command(buffer, packet_info, vsomeip_tree)
    local client_id_field_size = 2
    if buffer:len() ~= client_id_field_size then
        -- TODO: Add error message if buffer is actually bigger than expected -> Malformed packet
    end
    local vsomeip_client_id = buffer(0, client_id_field_size)
    vsomeip_tree:add_le(vsomeip_newclient, vsomeip_client_id)
end

-- Supports VSOMEIP_REGISTER_APPLICATION structure with:
--  - Port (2 bytes)
function register_application_command(buffer, packet_info, vsomeip_tree)
    local port_field_size = 2
    if buffer:len() ~= port_field_size then
        -- TODO: Add error message if buffer is actually bigger than expected -> Malformed packet
    end
    local register_app_port = buffer(0, port_field_size)
    vsomeip_tree:add_le(vsomeip_port, register_app_port)
end

-- Supports VSOMEIP_ROUTING_INFO structure with:
--  Entries
--    SubCommand (1 byte); RIE_ADD_CLIENT (0x0) or RIE_DEL_CLIENT (0x1)
--    Size (4 bytes)
--    Client (2 bytes)
--    [Address] (variable length)
--    [Port] (2 bytes)
--    OR
--    SubCommand (1 byte); RIE_ADD_SERVICE_INSTANCE (0x2) or RIE_DEL_SERVICE_INSTANCE (0x4)
--    Command Size  (4 bytes)
--    Client infoSize (4 bytes)
--    Client (2 bytes)
--    [Address] (variable length)
--    [Port] (2 bytes)
--    Size (4 bytes)
--        Service (2 bytes)
--        Instance (2 bytes)
--        Major (1 byte)
--        Minor (4 bytes)
function routing_info_command(buffer, packet_info, vsomeip_tree)
    local routing_info_tree = vsomeip_tree:add("Routing Info Commands")
    local MIN_ROUTING_INFO_SIZE = 5
    while buffer ~= nil and buffer:len() >= MIN_ROUTING_INFO_SIZE do
        local cursor = 0
        routing_info_command_field_size = 1
        local routing_info_command = buffer(cursor, routing_info_command_field_size)
        cursor = cursor + routing_info_command_field_size
        local routing_command_name = get_routing_info_command_name(routing_info_command:le_uint())
        routing_info_size_field_size = 4
        local routing_info_size = buffer(cursor, routing_info_size_field_size)
        cursor = cursor + routing_info_size_field_size

        routing_info_tree_cmd = routing_info_tree:add_le(vsomeip_routing_info_command, routing_info_command):append_text(" ("..routing_command_name .. ")")
        routing_info_tree_cmd:add_le(vsomeip_routing_info_size, routing_info_size)
        if is_routing_service_instance_command(routing_command_name) then
            cursor = load_routing_service_instance_command(buffer, cursor, routing_info_tree_cmd)
        else
            local client_buffer = buffer(cursor, routing_info_size:le_uint())
            load_routing_info_client(client_buffer, routing_info_tree_cmd)
            cursor = cursor + routing_info_size:le_uint()
        end

        buffer = try_seek(buffer, cursor)
    end
end

-- Supports VSOMEIP_OFFER_SERVICE and VSOMEIP_STOP_OFFER_SERVICE structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Major (1 byte)
--  - Minor (4 bytes)
function offer_service_and_stop_offer_service_commands(buffer, packet_info, vsomeip_tree)
    local service_info, buffer_cursor = read_service_info(buffer, 0)
    vsomeip_tree:add_le(vsomeip_service, service_info.service_id)
    vsomeip_tree:add_le(vsomeip_instance, service_info.instance_id)
    vsomeip_tree:add_le(vsomeip_major, service_info.major_version)
    vsomeip_tree:add_le(vsomeip_minor, service_info.minor_version)
    packet_info.cols.info:append(" [" .. service_info.service_id(1,1)..service_info.service_id(0,1) ..".".. service_info.instance_id(1,1)..service_info.instance_id(0,1) .. "]")
end

-- Supports VSOMEIP_SUBSCRIBE structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Eventgroup (2 bytes)
--  - Major (1 byte)
--  - Event (2 bytes)
--  - Pending ID (2 bytes)
--  - Filter
--  -     OnChange (1 byte)
--  -     OnChangeResetsInterval (1 byte)
--  -     Interval (8 bytes)
--  -     Ignore (per entry)
--  -         Key (8 bytes)
--  -         Value (1 byte)
function subscribe_command(buffer, packet_info, vsomeip_tree)
    local major_version
    local service_id, instance_id, buffer_cursor = read_service_instance_info(buffer, 0)
    local eventgroup_id_field_size = 2
    local eventgroupd_id = buffer(buffer_cursor, eventgroup_id_field_size)
    buffer_cursor = buffer_cursor + eventgroup_id_field_size
    major_version, buffer_cursor = read_someip_major_version(buffer, buffer_cursor)
    local event_id_field_size = 2
    local event_id = buffer(buffer_cursor, event_id_field_size)
    buffer_cursor = buffer_cursor + event_id_field_size
    local pending_id_field_size = 2
    local pending_id = buffer(buffer_cursor, pending_id_field_size)
    buffer_cursor = buffer_cursor + pending_id_field_size

    vsomeip_tree:add_le(vsomeip_service, service_id)
    vsomeip_tree:add_le(vsomeip_instance, instance_id)
    vsomeip_tree:add_le(vsomeip_eventgroup, eventgroupd_id)
    vsomeip_tree:add(vsomeip_major, major_version)
    vsomeip_tree:add_le(vsomeip_event, event_id)
    vsomeip_tree:add_le(vsomeip_id, pending_id)

    local filter_field_size = buffer:len() - buffer_cursor
    if filter_field_size > 0 then
        local filter = buffer(buffer_cursor, filter_field_size)
        load_subscribe_filter(buffer, buffer_cursor, filter_field_size, vsomeip_tree)
        buffer_cursor = buffer_cursor + filter_field_size
    end

    packet_info.cols.info:append(" [" .. service_id(1,1)..service_id(0,1) ..".".. instance_id(1,1)..instance_id(0,1)
                        .. "." .. eventgroupd_id(1,1)..eventgroupd_id(0,1) .. "." .. event_id(1,1)..event_id(0,1) .. "]")
end

-- Supports VSOMEIP_UNSUBSCRIBE AND VSOMEIP_EXPIRE structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Eventgroup (2 bytes)
--  - Event (2 bytes)
--  - Pending ID (2 bytes)
function unsubscribe_or_expire_commands(buffer, packet_info, vsomeip_tree)
    local service_id, instance_id, buffer_cursor = read_service_instance_info(buffer, 0)
    local eventgroup_id_field_size = 2
    local eventgroupd_id = buffer(buffer_cursor, eventgroup_id_field_size)
    buffer_cursor = buffer_cursor + eventgroup_id_field_size
    local event_id_field_size = 2
    local event_id = buffer(buffer_cursor, event_id_field_size)
    buffer_cursor = buffer_cursor + event_id_field_size
    -- TODO Is this ID different from the one on the VSOMEIP_SUBSCRIBE ?
    local id_field_size = 2
    local id = buffer(buffer_cursor, id_field_size)
    buffer_cursor = buffer_cursor + id_field_size

    vsomeip_tree:add_le(vsomeip_service, service_id)
    vsomeip_tree:add_le(vsomeip_instance, instance_id)
    vsomeip_tree:add_le(vsomeip_eventgroup, eventgroupd_id)
    vsomeip_tree:add_le(vsomeip_event, event_id)
    vsomeip_tree:add_le(vsomeip_id, id_field_size)

    packet_info.cols.info:append(" [" .. service_id(1,1)..service_id(0,1) ..".".. instance_id(1,1)..instance_id(0,1)
                        .. "." .. eventgroupd_id(1,1)..eventgroupd_id(0,1) .. "." .. event_id(1,1)..event_id(0,1) .. "]")
end

-- Supports VSOMEIP_REQUEST_SERVICE structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Major (1 byte)
--  - Minor (4 bytes)
function request_service_command(buffer, packet_info, vsomeip_tree)
    local service_info_size = 9
    local total_services_info = buffer:len() / service_info_size
    local buffer_cursor = 0
    if total_services_info > 0 then
        local services_requested_tree = vsomeip_tree:add("Services")
        for i = 0, total_services_info - 1,1 do
            local service_info, next_position = read_service_info(buffer, buffer_cursor)
            buffer_cursor = next_position
            local this_service_tree = services_requested_tree:add(i .. " [" .. service_info.service_id(1,1)..service_info.service_id(0,1) ..".".. service_info.instance_id(1,1)..service_info.instance_id(0,1) .. "]")
            this_service_tree:add_le(vsomeip_service, service_info.service_id)
            this_service_tree:add_le(vsomeip_instance, service_info.instance_id)
            this_service_tree:add_le(vsomeip_major, service_info.major_version)
            this_service_tree:add_le(vsomeip_minor, service_info.minor_version)
        end
    end
end

-- Supports VSOMEIP_RELEASE_SERVICE structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
function release_service_command(buffer, packet_info, vsomeip_tree)
    local service_id, instance_id, buffer_cursor = read_service_instance_info(buffer, 0)
    vsomeip_tree:add_le(vsomeip_service, service_id)
    vsomeip_tree:add_le(vsomeip_instance, instance_id)
    packet_info.cols.info:append(" [" .. service_id(1,1)..service_id(0,1) ..".".. instance_id(1,1)..instance_id(0,1) .. "]")
end

-- Supports VSOMEIP_SUBSCRIBE_NACK and VSOMEIP_SUBSCRIBE_ACK structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Eventgroup (2 bytes)
--  - Subscriber (2 bytes)
--  - Event (2 bytes)
--  - ID (2 bytes)
function subscribe_nack_or_ack_commands(buffer, packet_info, vsomeip_tree)
    local service_id, instance_id, buffer_cursor = read_service_instance_info(buffer, 0)
    local eventgroup = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local subscriber = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local event = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local id = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2

    vsomeip_tree:add_le(vsomeip_service, service_id)
    vsomeip_tree:add_le(vsomeip_instance, instance_id)
    vsomeip_tree:add_le(vsomeip_eventgroup, eventgroup)
    vsomeip_tree:add_le(vsomeip_subscriber, subscriber)
    vsomeip_tree:add_le(vsomeip_event, event)
    vsomeip_tree:add_le(vsomeip_id, id)

    packet_info.cols.info:append(" [" .. service_id(1,1)..service_id(0,1) ..".".. instance_id(1,1)..instance_id(0,1)
                        .. "." .. eventgroup(1,1)..eventgroup(0,1) .. "." .. event(1,1)..event(0,1)
                        .. "] (" .. subscriber(1,1)..subscriber(0,1) .. ")")
end

-- Supports VSOMEIP_SEND, VSOMEIP_NOTIFY and VSOMEIP_NOTIFY_ONE structure by using standard SOME/IP dissector.
function someip_encapsulated_in_vsomeip_commands(buffer, packet_info, vsomeip_tree, root_tree)
    local instance_field_size = 2
    local reliability_field_size = 1
    local crc_field_size = 1
    local dstClient_field_size = 2
    vsomeip_tree:add_le(vsomeip_instance, buffer(0, instance_field_size))
    local cursor = instance_field_size
    local reliable_field = buffer(cursor, reliability_field_size)
    vsomeip_tree:add(vsomeip_reliable, reliable_field):append_text(" (" .. get_reliability(reliable_field:uint()) .. ")")
    cursor = cursor + reliability_field_size
    vsomeip_tree:add(vsomeip_crc, buffer(cursor, crc_field_size))
    cursor = cursor + crc_field_size
    local dstClient_field = buffer(cursor, dstClient_field_size)
    vsomeip_tree:add_le(vsomeip_dstClient, dstClient_field)
    cursor = cursor + dstClient_field_size
    -- The rest of the packet contains a SOME/IP payload
    Dissector.get('someip_tcp'):call(buffer(cursor, buffer:len() - cursor):tvb(), packet_info, root_tree)
end

-- Supports VSOMEIP_REGISTER_EVENT structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Notifier (2 bytes)
--  - Type (1 byte); ET_EVENT (00), ET_SELECTIVE_EVENT(01) or ET_FIELD(02)
--  - Provided (1 byte); False (00) or True (01)
--  - Reliability  (1 byte); UDP (00) or TCP (01)
--  - IsCyclic  (1 byte)
--  - Num Eventgroups  (2 bytes)
--  - Entries
--  -     Eventgroup (2 bytes)
function register_event_command(buffer, packet_info, vsomeip_tree)
    if buffer:len() > 0 then
        local register_event_tree = vsomeip_tree:add("Services")

        local index = 0
        local MIN_REGISTER_EVENT_SIZE = 12
        while buffer ~= nil and buffer:len() >= MIN_REGISTER_EVENT_SIZE do
            local cursor = 0
            local register_event_service_id = buffer(cursor, 2)
            cursor = cursor + 2
            local register_event_instance = buffer(cursor, 2)
            cursor = cursor + 2
            local register_event_notifier = buffer(cursor, 2)
            cursor = cursor + 2
            local register_event_type = buffer(cursor, 1)
            cursor = cursor + 1
            local register_event_provided = buffer(cursor, 1)
            cursor = cursor + 1
            local register_event_reliability = buffer(cursor, 1)
            cursor = cursor + 1
            local register_event_iscyclic = buffer(cursor, 1)
            cursor = cursor + 1
            local register_event_numeventgroups = buffer(cursor, 2)
            cursor = cursor + 2

            local register_event_tree_service_tree = register_event_tree:add(index .. ": [" .. register_event_service_id(1,1)..register_event_service_id(0,1) ..".".. register_event_instance(1,1)..register_event_instance(0,1) .. "]")
            register_event_tree_service_tree:add_le(vsomeip_register_event_service, register_event_service_id)
            register_event_tree_service_tree:add_le(vsomeip_register_event_instance, register_event_instance)
            register_event_tree_service_tree:add_le(vsomeip_register_event_notifier, register_event_notifier)
            register_event_tree_service_tree:add(vsomeip_register_event_type, register_event_type):append_text(" (" .. get_register_event_type_name(register_event_type:uint()) .. ")")
            register_event_tree_service_tree:add(vsomeip_register_event_provided, register_event_provided)
            register_event_tree_service_tree:add(vsomeip_register_event_reliability, register_event_reliability):append_text(" (" .. get_reliability(register_event_reliability:uint()) .. ")")
            register_event_tree_service_tree:add(vsomeip_register_event_iscyclic, register_event_iscyclic)
            register_event_tree_service_tree:add_le(vsomeip_register_event_numeventgroups, register_event_numeventgroups)

            for i = 0, register_event_numeventgroups:le_uint() - 1, 1 do
                local register_event_eventgroup = buffer(cursor, 2)
                cursor = cursor + 2
                register_event_tree_service_tree:add_le(vsomeip_register_event_eventgroup, register_event_eventgroup)
            end

            buffer = try_seek(buffer, cursor)
            index = index + 1
        end
    end
end

-- Supports VSOMEIP_UNREGISTER_EVENT structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Notifier (2 bytes)
--  - Provided (1 byte)
function unregister_event_command(buffer, packet_info, vsomeip_tree)
    local command_service = buffer(0, 2)
    local buffer_cursor = 2
    local command_instance = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local command_notifier = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local command_provided = buffer(buffer_cursor, 1)
    buffer_cursor = buffer_cursor + 1

    vsomeip_tree:add_le(vsomeip_service, command_service)
    vsomeip_tree:add_le(vsomeip_instance, command_instance)
    vsomeip_tree:add_le(vsomeip_notifier, command_notifier)
    vsomeip_tree:add(vsomeip_provided, command_provided)

    packet_info.cols.info:append(" [" .. command_service(1,1)..command_service(0,1) ..".".. command_instance(1,1)..command_instance(0,1) .. "]")
end

-- Supports VSOMEIP_OFFERED_SERVICES_REQUEST structure with:
--  - OfferType (1 byte) (00 = LOCAL, 01 = REMOTE, 02 = ALL)
function offered_services_request_command(buffer, packet_info, vsomeip_tree)
    local command_offer_type = buffer(0, 1)
    local command_offer_type_name = get_offer_type(command_offer_type:uint())
    vsomeip_tree:add(vsomeip_offer_type, command_offer_type):append_text(" (" .. command_offer_type_name .. ")")
    packet_info.cols.info:append(" (" .. command_offer_type_name .. ")")
end

-- Supports VSOMEIP_OFFERED_SERVICES_RESPONSE structure with:
--  - OfferedServices
--  -     Subcommand (1 byte) (00 = ADD CLIENT, 01 = ADD SERVICE INSTANCE, 02 = DELETE SERVICE INSTANCE, 03 = DELETE CLIENT)
--  -     Size (4 bytes)
--  -     ServiceInstances
--  -         Service (2 bytes)
--  -         Instance (2 bytes)
--  -         Major (2 bytes)
--  -         Minor (2 bytes)
-- TODO: Implement OfferedServices parsing
function offered_services_response_command(buffer, packet_info, vsomeip_tree)
    local offered_services = buffer(0, buffer:len())
    vsomeip_tree:add(vsomeip_offered_services, offered_services)
end

-- Supports VSOMEIP_UNSUBSCRIBE_ACK structure with:
--  - Service (2 bytes)
--  - Instance (2 bytes)
--  - Eventgroup (2 bytes)
--  - ID (2 bytes)
function unsubscribe_ack_command(buffer, packet_info, vsomeip_tree)
    local command_service = buffer(0, 2)
    local buffer_cursor = 2
    local command_instance = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local command_eventgroup = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2
    local command_id = buffer(buffer_cursor, 2)
    buffer_cursor = buffer_cursor + 2

    vsomeip_tree:add_le(vsomeip_service, command_service)
    vsomeip_tree:add_le(vsomeip_instance, command_instance)
    vsomeip_tree:add_le(vsomeip_eventgroup, command_eventgroup)
    vsomeip_tree:add_le(vsomeip_id, command_id)

    packet_info.cols.info:append(" [" .. command_service(1,1)..command_service(0,1) ..".".. command_instance(1,1)..command_instance(0,1)
                        .. "." .. command_eventgroup(1,1)..command_eventgroup(0,1) .. "]")
end

-- Supports VSOMEIP_RESEND_PROVIDED_EVENTS structure with:
--  - PendingOfferId (4 bytes)
function resend_provided_events_command(buffer, packet_info, vsomeip_tree)
    local pending_offer_id = buffer(0, 4)
    vsomeip_tree:add_le(vsomeip_pending_offer_id, pending_offer_id)
end

-- Supports VSOMEIP_UPDATE_SECURITY_POLICY and VSOMEIP_UPDATE_SECURITY_POLICY_INT structure with:
--  - UpdateId (4 bytes)
--  - Policy (variable length)
-- TODO: Implement Policy parsing
function update_sec_policy_commands(buffer, packet_info, vsomeip_tree)
    local update_id_field_size = 4
    local update_id = buffer(0, update_id_field_size)
    local policy = buffer(update_id_field_size, buffer:len() - update_id_field_size)
    vsomeip_tree:add_le(vsomeip_update_id, update_id)
    vsomeip_tree:add(vsomeip_policy, policy)
end

-- Supports VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE and VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE structure with:
--  - UpdateId (4 bytes)
function update_or_remove_sec_policy_response_commands(buffer, packet_info, vsomeip_tree)
    local update_id = buffer(0, 4)
    vsomeip_tree:add_le(vsomeip_update_id, update_id)
end

-- Supports VSOMEIP_REMOVE_SECURITY_POLICY structure with:
--  - UpdateId (4 bytes)
--  - Uid (4 bytes)
--  - Gid (4 bytes)
function remove_sec_policy_command(buffer, packet_info, vsomeip_tree)
    local update_id = buffer(0, 4)
    local uid = buffer(4, 4)
    local gid = buffer(8, 4)
    vsomeip_tree:add_le(vsomeip_update_id, update_id)
    vsomeip_tree:add_le(vsomeip_uid, uid)
    vsomeip_tree:add_le(vsomeip_gid, gid)
end

-- Supports VSOMEIP_UPDATE_SECURITY_CREDENTIALS structure with:
--  - Credentials
--    - Uid (4 bytes)
--    - Gid (4 bytes)
-- TODO: Implement Credentials parsing
function update_sec_credentials_command(buffer, packet_info, vsomeip_tree)
    local credentials = buffer(0, buffer:len())
    vsomeip_tree:add(vsomeip_credentials, credentials)
end

-- Supports VSOMEIP_DISTRIBUTE_SECURITY_POLICIES structure with:
--  - PoliciesCount (4 bytes)
--  - Policies
--    - Size (4 bytes)
--    - Data (variable length)
-- TODO: Implement policies parsing
function distribute_sec_credentials_command(buffer, packet_info, vsomeip_tree)
    local policy_count_field_size = 4
    local policy_count = buffer(0, policy_count_field_size)
    local policies = buffer(policy_count_field_size, buffer:len() - policy_count_field_size)
    vsomeip_tree:add_le(vsomeip_policy_count, policy_count)
    vsomeip_tree:add(vsomeip_policies, policies)
end

-- Supports VSOMEIP_CONFIG structure with:
--  - Configurations
--  -     Key Size (4 bytes)
--  -     Key (variable length)
--  -     Value Size (4 bytes)
--  -     Value (variable length)
function config_command(buffer, packet_info, vsomeip_tree)
    local configurations_key_size = buffer(0, 4)
    local buffer_cursor = 4
    local configurations_key = buffer(buffer_cursor, configurations_key_size:le_uint())
    buffer_cursor = buffer_cursor + configurations_key_size:le_uint()
    local configurations_value_size = buffer(buffer_cursor, 4)
    buffer_cursor = buffer_cursor + 4
    local configurations_value = buffer(buffer_cursor, configurations_value_size:le_uint())
    buffer_cursor = buffer_cursor + configurations_value_size:le_uint()

    vsomeip_tree:add_le(vsomeip_configurations_key_size, configurations_key_size)
    vsomeip_tree:add(vsomeip_configurations_key, configurations_key)
    vsomeip_tree:add_le(vsomeip_configurations_value_size, configurations_value_size)
    vsomeip_tree:add(vsomeip_configurations_value, configurations_value)
end

function calc_vsomeip_message_size(command_name, message_size_field)
    return vsomeip_extra_header_sized + message_size_field
end

-- Common function for all commands
function common_vsomeip_header(command_name, command_id, buffer, packet_info, tree)
    -- Version
    local version_field = buffer(0, version_field_size)
    local buffer_cursor = version_field_size
    local client_field
    -- Client
    client_field = buffer(buffer_cursor, client_field_size)
    buffer_cursor = buffer_cursor + client_field_size
    -- Size
    local size_field = buffer(buffer_cursor, size_field_size)
    local message_size = size_field:le_uint()

    local vsomeip_command_subtree = tree:add(vsomeip_protocol, buffer(), "vsomeip3 (" .. calc_vsomeip_message_size(command_name, message_size) .. " bytes) " .. command_name)
    vsomeip_command_subtree:add_le(vsomeip_command, command_id):append_text(" (" .. command_name .. ")")
    vsomeip_command_subtree:add_le(vsomeip_version, version_field)
    if command_name ~= "VSOMEIP_SUSPEND" then
        packet_info.cols.info:append(" (" ..client_field(1,1)..client_field(0,1).. ")")
    end
    vsomeip_command_subtree:add_le(vsomeip_client, client_field)
    vsomeip_command_subtree:add_le(vsomeip_size, size_field)

    return message_size, vsomeip_command_subtree
end

-- Finds the structure command for a given command_id
-- The structure command is a table with the following fields:
--  - name: The name of the command
--  - (optional) func: Handling command function
--
-- The handling command receives as arguments the following:
--  - buffer: The buffer with the command starting after the vsomeip_size field and with len equal to the message size
--  - packet_info: The packet information
--  - vsomeip_tree: The tree where the vsomeip command fields shall be added
--  - root_tree: The root tree of the packet (root of vsomeip_tree)
--
-- Arguments:
--  - command_id: The command id to find the structure for
-- Return:
--  - The structure command table
function find_command_structure(command_id)
    local command_table = {
        [0x00] = {["name"] = "VSOMEIP_ASSIGN_CLIENT", ["handler"] = assign_client_command},
        [0x01] = {["name"] = "VSOMEIP_ASSIGN_CLIENT_ACK", ["handler"] = assign_client_ack_command},
        [0x02] = {["name"] = "VSOMEIP_REGISTER_APPLICATION", ["handler"] = register_application_command},
        [0x03] = {["name"] = "VSOMEIP_DEREGISTER_APPLICATION"},
        [0x04] = {["name"] = "VSOMEIP_APPLICATION_LOST"},
        [0x05] = {["name"] = "VSOMEIP_ROUTING_INFO", ["handler"] = routing_info_command},
        [0x06] = {["name"] = "VSOMEIP_REGISTERED_ACK"},
        [0x07] = {["name"] = "VSOMEIP_PING"},
        [0x08] = {["name"] = "VSOMEIP_PONG"},
        [0x10] = {["name"] = "VSOMEIP_OFFER_SERVICE", ["handler"] = offer_service_and_stop_offer_service_commands},
        [0x11] = {["name"] = "VSOMEIP_STOP_OFFER_SERVICE", ["handler"] = offer_service_and_stop_offer_service_commands},
        [0x12] = {["name"] = "VSOMEIP_SUBSCRIBE", ["handler"] = subscribe_command},
        [0x13] = {["name"] = "VSOMEIP_UNSUBSCRIBE", ["handler"] = unsubscribe_or_expire_commands},
        [0x14] = {["name"] = "VSOMEIP_REQUEST_SERVICE", ["handler"] = request_service_command},
        [0x15] = {["name"] = "VSOMEIP_RELEASE_SERVICE", ["handler"] = release_service_command},
        [0x16] = {["name"] = "VSOMEIP_SUBSCRIBE_NACK", ["handler"] = subscribe_nack_or_ack_commands},
        [0x17] = {["name"] = "VSOMEIP_SUBSCRIBE_ACK", ["handler"] = subscribe_nack_or_ack_commands},
        [0x18] = {["name"] = "VSOMEIP_SEND", ["handler"] = someip_encapsulated_in_vsomeip_commands},
        [0x19] = {["name"] = "VSOMEIP_NOTIFY", ["handler"] = someip_encapsulated_in_vsomeip_commands},
        [0x1A] = {["name"] = "VSOMEIP_NOTIFY_ONE", ["handler"] = someip_encapsulated_in_vsomeip_commands},
        [0x1B] = {["name"] = "VSOMEIP_REGISTER_EVENT", ["handler"] = register_event_command},
        [0x1C] = {["name"] = "VSOMEIP_UNREGISTER_EVENT", ["handler"] = unregister_event_command},
        [0x1D] = {["name"] = "VSOMEIP_ID_RESPONSE"},
        [0x1E] = {["name"] = "VSOMEIP_ID_REQUEST"},
        [0x1F] = {["name"] = "VSOMEIP_OFFERED_SERVICES_REQUEST", ["handler"] = offered_services_request_command},
        [0x20] = {["name"] = "VSOMEIP_OFFERED_SERVICES_RESPONSE", ["handler"] = offered_services_response_command},
        [0x21] = {["name"] = "VSOMEIP_UNSUBSCRIBE_ACK", ["handler"] = unsubscribe_ack_command},
        [0x22] = {["name"] = "VSOMEIP_RESEND_PROVIDED_EVENTS", ["handler"] = resend_provided_events_command},
        [0x23] = {["name"] = "VSOMEIP_UPDATE_SECURITY_POLICY", ["handler"] = update_sec_policy_commands},
        [0x24] = {["name"] = "VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE", ["handler"] = update_or_remove_sec_policy_response_commands},
        [0x25] = {["name"] = "VSOMEIP_REMOVE_SECURITY_POLICY", ["handler"] = remove_sec_policy_command},
        [0x26] = {["name"] = "VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE", ["handler"] = update_or_remove_sec_policy_response_commands},
        [0x27] = {["name"] = "VSOMEIP_UPDATE_SECURITY_CREDENTIALS", ["handler"] = update_sec_credentials_command},
        [0x28] = {["name"] = "VSOMEIP_DISTRIBUTE_SECURITY_POLICIES"},
        [0x29] = {["name"] = "VSOMEIP_UPDATE_SECURITY_POLICY_INT", ["handler"] = update_sec_policy_commands},
        [0x2A] = {["name"] = "VSOMEIP_EXPIRE", ["handler"] = unsubscribe_or_expire_commands},
        [0x30] = {["name"] = "VSOMEIP_SUSPEND"},
        [0x31] = {["name"] = "VSOMEIP_CONFIG", ["handler"] = config_command}
    }
    return command_table[command_id]
end

-- Command handling
-- Returns the end position of the command
-- If command is not found returns -1
function try_read_vsomeip_packet(buffer, packet_info, tree)
    local command_field_size = 1
    local command_id = buffer(magic_cookie_size, command_field_size)
    local buffer_cursor = magic_cookie_size + command_field_size
    local MINIMAL_VSOMEIP_PAYLOAD_LENGTH = 11
    if buffer:len() < MINIMAL_VSOMEIP_PAYLOAD_LENGTH then
        -- Command is not big enough to be a vsomeip command
        return -1
    end
    local command = find_command_structure(command_id:uint())
    if (command) then
        packet_info.cols.protocol = "vsomeip3"
        packet_info.cols.info = command["name"]
        -- Message size starts counting on the buffer_cursor value returned by the common_vsomeip_header
        local message_size, vsomeip_command_subtree = common_vsomeip_header(command["name"], command_id, try_seek(buffer, buffer_cursor), packet_info, tree)
        if (not command["handler"] or message_size == 0) then
            return calc_vsomeip_message_size(command[name], message_size)
        end
        command["handler"](buffer(pre_header_size, message_size), packet_info, vsomeip_command_subtree, tree)
        return vsomeip_extra_header_sized + message_size
    else
        -- Undefined command
        return -1
    end
end

-- Check if it is a vsomeip packet by verifying the start and end of the packet
function is_vsomeip_packet(buffer)
    if buffer:len() < vsomeip_extra_header_sized then
        return false
    end
    -- Magic cookie used at the start and end of the packet to identify if it's a vsomeip packet
    local VSOMEIP_START = 0x67376D07
    local VSOMEIP_END = 0x076D3767
    return buffer(0, magic_cookie_size):uint() == VSOMEIP_START and buffer(buffer:len() - magic_cookie_size, magic_cookie_size):uint() == VSOMEIP_END
end

function vsomeip_protocol.dissector(buffer, packet_info, root_tree)
    local vsomeip_packets_counter = 0
    -- Limitations:
    -- * TCP stream only contains vsomeip packets
    -- * TCP stream is correctly splitted and contains the full vsomeip packet
    while buffer ~= nil do
        -- Check if it's a vsomeip packet
        if not is_vsomeip_packet(buffer) then
            return
        end

        end_packet_position = try_read_vsomeip_packet(buffer, packet_info, root_tree)
        if end_packet_position == -1 then
            -- TODO: Malformed packet
            return
        end
        vsomeip_packets_counter = vsomeip_packets_counter + 1
        buffer = try_seek(buffer, end_packet_position)
    end
    if vsomeip_packets_counter > 1 then
        packet_info.cols.info = " VSOMEIP3 [This TCP stream contains " .. vsomeip_packets_counter .. " vsomeip packets]"
    end
end

DissectorTable.get('tcp.port'):add('1-65535', vsomeip_protocol)
