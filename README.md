# pico-vt
Terminal emulator

This is an ANSI terminal emulator for embeded projects bases loosely on [libtmt](https://github.com/deadpixi/libtmt).

There are specific (possibly not good) reasons for this library existing:
* I need cell (character & attributes) encoding/decoding to be fast as I am usinging it in a beam chasing renderer.
* I need it to have a small memory footprint and no use of malloc.
* I want it to be testable so I can correct mistakes and extend it.

The tests are very simple and do not rely on a framework (I am so tired of trying to get old testing frameworks to function).

The code is not complete, but I am happy to recieve pull requests for changes... particularly if they have a test included.

