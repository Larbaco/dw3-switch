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

Do **not** probe TCP `28280` (nmap/nc/connect) before `nxlink -s`. An empty
connect breaks hbmenu NetLoader with `err=11`. Prefer Sphaira **Y** NetLoader,
then run `nxlink -a <ip> -s …` directly.

Historical note from this check: TCP port `28280` accepted a connection. The
first attempt, using a local preview image while the owner was interacting
with the Switch, exited with `recv: Connection reset by peer` /
`Invalid response`.

A retry with the official [preview 7 NXLink NRO](https://github.com/Larbaco/dw3-switch/releases/download/v0.6.5-preview.7/dw3-switch-nxlink.nro)
succeeded. The release SHA-256 was verified as
`cc9d50da954db98acc9f3d38840bd602e809d9c2d66d40ab499e5d871160dec4`.
The NXLink client reported `Sending dw3-switch-nxlink.nro, 8964293 bytes`,
then `server active ...` and `nxlink stdio connected (fd=3)`. The NRO booted
on the OLED and emitted game logs, confirming transfer, launch, and the live
stdio callback. The host-side client was then stopped; that did not stop the
homebrew running on the Switch.

Command used (the release asset was mounted read-only):

```sh
docker run --rm --network host \\
  -v /tmp/dw3-preview7-nxlink:/nro:ro \\
  devkitpro/devkita64:latest \\
  nxlink -a 192.168.31.235 -s /nro/dw3-switch-nxlink.nro
```

For the project's NXLink development build flow, see the [Switch build instructions](../platforms/switch/README.md#development-build-with-live-nxlink-logs).
No FTP write operation was attempted.

## Recheck after DHCP changes

Probe FTP only. Do **not** include `28280` in nmap while NetLoader may be
waiting (see NXLink above).

```sh
nmap -Pn -n -sT -p 21,5000 <switch-ip>
curl --max-time 5 --ftp-pasv ftp://<switch-ip>:5000/
```

Results from this check: host reachable; `21/tcp` closed, `5000/tcp` open.
UDP was not scanned. `28280` was observed open earlier during an active
nxlink session only — do not scan it as a liveness check.
