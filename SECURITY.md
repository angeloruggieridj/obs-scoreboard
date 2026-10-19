# Security policy

## Supported versions

Only the latest release receives fixes.

## Reporting a vulnerability

Please do **not** open a public issue for a security problem. Use GitHub's private reporting:
**Security → Report a vulnerability** on <https://github.com/angeloruggieridj/obs-scoreboard/security/advisories/new>.

Include the plugin version, your OBS Studio version and system, and the steps to reproduce. You can expect an
answer within a few days.

## What the plugin does on the network

- **Update check** — once per OBS start, an HTTPS `GET` to the GitHub releases API of this repository (only a
  `User-Agent` with the plugin version is sent). It never downloads or installs anything; it can be switched off from
  the dock menu.
- **Remote control** — the plugin opens no port. It registers a vendor with obs-websocket (if present); access is
  governed by the obs-websocket server settings and password.

Release packages are not code-signed; verify them with the SHA-256 checksums and the build provenance attestation
(`gh attestation verify <file> --repo angeloruggieridj/obs-scoreboard`).
