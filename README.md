mod2mus
=======

A tool for converting ProTracker MOD files (`1CHN`, `2CHN`, `3CHN` and `M.K.`
magic bytes) to the MUS format used in Psycho Pinball and Micro Machines 2 (PC).

More information on the MUS format can be found at the
[ModdingWiki](http://www.shikadi.net/moddingwiki/Karl_Morton_Music_Format).

Conversion notes
================

- This tool only writes on SONG chunk. Its name is taken from the MOD name, so
  it mustn't be empty in order for the games to be able to find a playable song.
- The MUS pattern format is strictly linear. Complicated pattern jumps will not
  be resolved, but simple pattern breaks or jumps to later order positions are
  supported.

Code notes
==========

- Written in C++20, compiles with Visual Studio 2019 but should work with any
  other C++20 compiler as well.

Copyright
=========

mod2mus is written by Saga Musix and released under the BSD 3-Clause license. 
