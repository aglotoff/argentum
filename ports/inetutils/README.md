# Inetutils-2.5 port

Changes:

* Add `argentum` to the OS list in `build-aux/config.sub`
* In `traceroute.c`, set hint socket type to `SOCK_DGRAM` before calling
  `getaddrinfo`
* Do not re-generate man page for `traceroute`

Configuration:

* Do not install `logger`, `whois`, `rcp`, `rexec`, `rlogin`, `rsh`,
  `ifconfig` and servers

Problems:

* Skip `ifconfig` (requires `ioctl` calls for the Ethernet driver)
* Produces non-executable `ping` and `traceroute`
