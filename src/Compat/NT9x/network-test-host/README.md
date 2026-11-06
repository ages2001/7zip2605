# Private NT 3.1 network test host

These tools are test infrastructure, not installed application dependencies.
The test server requires Python and impacket. Create a `share` directory and
run `server.py` and `nbns.py` on the QEMU host. They bind loopback only, ports
139/TCP and 137/UDP. Guest access uses QEMU user networking at 10.0.2.2.
Use only a disposable share: the SMB server permits guest writes.

`andx_compat.py` adds SMB1 AndX request chaining, reports the disk-share
service as `A:`, and avoids advertising extended security to clients that
did not request it. Missing SMB_QUERY_INFORMATION paths return a missing-file
status instead of closing the connection. These fix test-server interoperability with NT 3.1;
they do not change 7-Zip or the installed Windows networking binaries.

For the native NT 3.1 AM2100/AM1500T/PCnet driver with QEMU's PCI PCnet at
00:03.0, set adapter BusType=1, IOBaseAddress=0xC000, InterruptNumber=11.
Keep DMAChannel=3. `pcnet-edge-rom.S` enables PCI bus mastering and selects
edge-triggered IRQ 11 before NT boots. This fixed PCI address and IRQ are
specific to the recorded QEMU layout; do not apply it to other layouts.
Build with GNU as --32 and objcopy -O binary -j .text, then set the last
byte to the two's complement of the sum of the other 511 bytes. Pass the
result as QEMU -option-rom. Use -device pcnet,netdev=net0,romfile= and
-netdev user,id=net0. The application itself does not require this ROM.

Guest TCP/IP: 10.0.2.15, mask 255.255.255.0, gateway 10.0.2.2.
Add `10.0.2.2 Z7HOST #PRE` to the guest's system32/drivers/etc/LMHOSTS;
run `nbtstat -R` to reload it. NT 3.1 still performs a direct NetBIOS name
query, answered by nbns.py. The test UNC is `\\Z7HOST\Z7TEST`.

The impacket server covers basic SMB file operations but its SRVSVC RPC
implementation does not interoperate with the NT 3.1 share-enumeration client.
For GUI testing use Samba 4.22 with `smb.conf.example` instead of server.py.
Adjust all workspace paths before starting it. Run smbd and samba-dcerpcd
with the same private configuration; pass the full paths of rpcd_classic
and rpcd_winreg to samba-dcerpcd. The latter is needed during classic RPC
service initialization. Keep nbns.py running. Do not use `force user` with
this rootless guest configuration: it changes the token SID and can deny
access to the guest-owned private share. This backend setup is test-only.

Run NETSMOKE.EXE for native MPR mapping/enumeration plus installed CLI
archive creation/test/extraction and byte comparison. Run
GUIROUND.EXE --network for the installed File Manager round trip.
Do not distribute raw SMB packet captures or server authentication logs;
publish only sanitized test reports and file hashes.
