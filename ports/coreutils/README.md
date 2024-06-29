# Coreutils-9.4 port

Changes:

* Add `argentum` to the OS list in `build-aux/config.sub`
* Change the `configure` script to not replace the `nanosleep` function
* Use `asnprintf(NULL, ...)` instead of `snprintf(nullptr, ...)` to prevent the
  segmentation violation (looks like glibc-specific behavior)

Configuration:
* Install `hostname` (disabled by default, but required by Perl)
* Do not install `kill` and `uptime` (will be installed by other packages)

Problems:

* Skip `sort` (requires `pthreads`)
* Skip `stty` (requires network support)
* `shuf` fails (`getrandom` not implemented)
* `pinky` hangs forever
* `users` hangs forever
