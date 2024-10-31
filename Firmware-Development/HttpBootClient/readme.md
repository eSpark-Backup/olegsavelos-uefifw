## NTFS Example
1. Create two partitions in a USB. One is FAT formatted and one is NTFS.
2. Place the contents of Binaries folder on the root of USB (FAT formatted partition).
```
##### USB Contents

/efi/boot/bootx64.efi
/efi/boot/ntfs_x64.efi
HttpBootClient.efi
```
3. Boot from USB (not in a VM).
4. The UEFI shell will be loaded.
5. Type fs0:
6. Type HttpBootClient and press enter.
7. TestFile should be created with a "Hello from UEFI" message on the NTFS partition.
 
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
