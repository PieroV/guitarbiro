# GuitarBiro
I wrote this program for an exam at University of Padova (Italy).

It recognizes monophonic notes that have been played in a guitar which has been plugged into the audio card.
Notes must be played individually, meaning that they must not overlap in time and that chords can't be recognized.

I've tested only on Debian, but I've written it to be cross platform.
The class required us to use CMake, so it should be easier to compile in all platforms, however this is my first CMake project, so I can't assure anything.

The program works only with standard-tuned guitar, but this hasn't been totally hard coded: there's a variable in `src/detect.c` which points to the standard tuning array, but you can change it.
I bet you could modify other parameters to detect bass guitar, too.

There are lots of things yet to do, but I don't know if I will do them.

## Kudos
Thanks to Gerry Beauregard for his [autocorrelation implementation](https://gerrybeauregard.wordpress.com/2013/07/15/high-accuracy-monophonic-pitch-estimation-using-normalized-autocorrelation/ "High Accuracy Monophonic Pitch Estimation Using Normalized Autocorrelation") which he released in the MIT license.

Thanks to @andrewrk for his libsoundio.

Thanks to all developers of GTK, RSVG and Cairo, too.

## License
This program is released in the GNU General Public License version 3, due to the usage of RSVG in `gui.c`.

If you don't need this file (or if you can remove the dependency on RSVG), you can consider the program released under the MIT License.
