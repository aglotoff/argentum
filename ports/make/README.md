# Make-4.4.1 port

Changes:

* Add `argentum` to the OS list in `build-aux/config.sub`
* Omit incompatible `getcwd` declaration in `src/makeint.h`

Configuration:
* Do not use `guile` from the build host
