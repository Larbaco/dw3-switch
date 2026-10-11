# Switch OLED network access

Hardware endpoint checked on 2026-10-11: `192.168.31.235`, with Sphaira
open. These notes describe the owner's local console only; the address is not
portable and may change with DHCP.

## FTP

Sphaira's FTP service is reachable on TCP port `5000`. An anonymous,
read-only directory-listing check succeeded. The root listed these mounts:

- `sdmc:`
- `album_nand:`
- `album_sd:`
- `install:`
- `games:`

The server replied that anonymous login succeeded. TCP port `21` was closed;
use port `5000`. No upload, delete, or other write operation was attempted.

Example read-only listing:

```sh
curl --max-time 5 --ftp-pasv ftp://<switch-ip>:5000/
```

The FTP server rejected EPSV but supported PASV; curl successfully fell back
to PASV and listed the root.

## NXLink

TCP port `28280` accepted a connection during the same check. This confirms
that a listener is reachable at the expected NXLink port, but does **not**
verify a full NXLink upload or live stdio session. The `nxlink` client is not
installed in this host environment, and no NRO was sent or launched. Do not
treat the open port alone as proof that Sphaira's NXLink transfer/callback works.

For game NRO development, the existing [Switch build instructions](../platforms/switch/README.md#development-build-with-live-nxlink-logs)
document the NXLink build and client flow. That flow can transfer and launch a
binary on the console, so confirm before using it on a live session. An FTP
probe and a TCP connect check are the non-writing checks performed here.

## Recheck after DHCP changes

```sh
nmap -Pn -n -sT -p 21,5000,28280 <switch-ip>
curl --max-time 5 --ftp-pasv ftp://<switch-ip>:5000/
```

Results from this check: host reachable; `21/tcp` closed, `5000/tcp` and
`28280/tcp` open. UDP was not scanned.
