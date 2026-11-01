# Contributing

Keep the project one file and zero dependencies: `src/c99gui.c` must compile as
ISO C99 with no extra libraries on every platform it claims to support.

Before opening a pull request:

1. Build with `make check` (or the equivalent flags in `.github/workflows/ci.yml`);
   the code must be warning-free under `-Wall -Wextra -Wpedantic -Wconversion
   -Wshadow -Wcast-qual -Wstrict-prototypes` and MSVC `/W4 /WX`.
2. Run `sh tests/smoke.sh <binary>`. Add a case for any behaviour you add.
3. Keep the source 7-bit ASCII (`grep -nP '[^\x00-\x7F]' src/c99gui.c` prints nothing).
4. Do not add compiler extensions, platform `#ifdef`s outside the PLATFORM LAYER,
   or heap allocation.
