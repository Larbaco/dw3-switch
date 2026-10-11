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

TCP port `28280` accepted a connection during the initial check. The host had
Docker and the `devkitpro/devkita64:latest` image, so the NXLink client was run
against the existing local game image (no build or file modification):

```sh
docker run --rm --network host \\
  -v /tmp/dw3-video-oled-build/switch:/nro:ro \\
  devkitpro/devkita64:latest \\
  nxlink -a 192.168.31.235 -s /nro/dw3-switch.nro
```

The client exited with status 1 and printed `recv: Connection reset by peer`
then `Invalid response`. Therefore, a TCP listener is reachable but an NXLink
upload/launch and live stdio session were **not** confirmed. The command is a
transfer-and-launch attempt; do not repeat it on a live session without
expecting it to replace the current Sphaira screen if accepted.

For the project's NXLink development flow, see the [Switch build instructions](../platforms/switch/README.md#development-build-with-live-nxlink-logs).
Sphaira documents NXLink as configurable under its Network options. Verify
that NXLink is enabled there before another test. No FTP write operation was
attempted.

## Recheck after DHCP changes

```sh
nmap -Pn -n -sT -p 21,5000,28280 <switch-ip>
curl --max-time 5 --ftp-pasv ftp://<switch-ip>:5000/
```

Results from this check: host reachable; `21/tcp` closed, `5000/tcp` and
`28280/tcp` open. UDP was not scanned.
