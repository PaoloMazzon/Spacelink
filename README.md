Spacelink
=========
Game for Ludum Dare 47. Its written entirely in one C file over the course of a weekend.
[Vulkan2D](https://github.com/PaoloMazzon/Vulkan2D), SDL2, and cute_sound were used to
make Spacelink.

Hardware Requirements
---------------------
Even though I didn't spend much time optimizing this because of the time constraints, it
will run on basically any consumer-end laptop or desktop released within the last 5ish years.
I wrote it and tested it on my laptop with an Intel Iris Plus integrated chip so it really
doesn't get much weaker than that while still supporting Vulkan.

 + Processor: 2ghz processor, 3rd gen i3
 + Graphics: Anything that supports Vulkan will run this
 + HDD: 30mb
 + RAM: 512mb (expect the game to use ~300mb tops)
 + Sound: Anything really

Building
========
Run build the cmake file. I tested and built on Windows only but it should be fine on Linux
too (since it uses Vulkan it will not run on OSX without some fiddling). Just make sure you
disable the Vulkan2D build option `VK2D_ENABLE_DEBUG` before building for release mode or it
will not run on most machines.