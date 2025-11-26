# vsomeip-dissector

Wireshark dissector for vSomeip internal communication via TCP

## How To Use

1. Place `vsomeip-dissector.lua` file in:
   - Linux: `~/.config/wireshark/plugins/vsomeip/vsomeip-dissector.lua`
   - Windows: `C:\Users\(username)\AppData\Roaming\Wireshark\plugins\`
   - (create `plugins` directory if it doesn't exist)
2. In wireshark go to `Analyze` > `Reload Lua Plugins`
3. In wireshark go to `Analyze` > `Enable Protocols` and search for `vsomeip3` and enable it

## References

vSomeip Protocol definitions: documentation/vsomeipProtocol.md