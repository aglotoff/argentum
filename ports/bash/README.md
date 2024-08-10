# Bash-5.2.32 port

Changes:

* Add `argentum` to the OS list in `build-aux/config.sub`
* Do not build the `getcwd` replacement (do not undef `HAVE_GETCWD` in
  `config-bot.h`)
* Since we don't have `_POSIX_VERSION` defined, add explicit checks for
  `__ARGENTUM__` to preprocessor conditionals in `include/posixwait.h`,
  `include/shtty.h`, `jobs.c`, and `lib/readline/rldefs.h`
* Add missing `defined (HAVE_SELECT)` check in `lib/readline/input.c`

Configuration:
* Do not use Bash's `malloc` (which is known to have bugs)

