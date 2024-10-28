# DHCP Server Settings VMware
1. Navigate to `C:\ProgramData\VMware`
2. Open vmnetdhcp.conf and copy these settings in the subnet section.

   `option tftp-server-name "<boot-server-ip>";  # Boot server IP (Option 66)`

   `option bootfile-name "<boot-config-path>";      # Boot file path (Option 67)`

### Example
```
subnet 192.168.82.0 netmask 255.255.255.0 {
range 192.168.82.128 192.168.82.254;            # default allows up to 125 VM's
option broadcast-address 192.168.82.255;
option domain-name-servers 192.168.82.2;
option domain-name "localdomain";
option netbios-name-servers 192.168.82.2;
option routers 192.168.82.2;
default-lease-time 1800;
max-lease-time 7200;
# DHCP Options for HTTP Boot Server
option tftp-server-name "192.168.18.30";  # Boot server IP (Option 66)
option bootfile-name "/boot-config";      # Boot file path (Option 67)
}
```
