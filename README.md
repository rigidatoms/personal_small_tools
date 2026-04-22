# personal_small_tools
For personal use. When programs don't offer the niche functionality you need, so you do it yourself.

Comments and messages might be in Portuguese, since this is a personal thing, more than anything else. But I'll still save these here since it's always useful to keep these ideas somewhere safer than my HD.

For now the only program present here is:

## add_loop_to_wav
Initially projected to be used for IMA ADPCM .wav files only, I decided to cover most cases, save for WAVE_FORMAT_EXTENSIBLE. Adds _smpl_ header to the end of the .wav, sets loop points so it can be used by any programs that support it, and adjusts RIFF size properly.
Usage (don't forget to change the parameters properly, haha): 
  ```
  ./build.sh
  ./main [input.wav] {loop_start value}
  ```
**ATTENTION:** some programs might not read correctly the data and produce silence at the end of the sample. Don't fret, as the data is still correct, might be a side effect to how they actually read the headers (from what I gathered). Players that actually process the sample size as expected don't cut off the data and play it correctly.
