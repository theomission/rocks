README
======

Controls
--------

Normal view:
------------

Space					Open menu 
Left Mouse Button 		Hold and drag to turn
Right Mouse Button 		Hold and drag to roll
Arrow up/down			Change time
Page up/down			Move up/down by 10 units
WASD					Move around
	Shift				Move faster
	Shift+Ctrl 			Move even faster

In menu:
--------

Enter					Activate item
Arrow keys				Move around in the menu
Backspace, left Arrow	Leave current item

when adjusting:
	up/down arrow keys		normal adjustment
	Shift					large adjustment
	Ctrl					small adjustment
	
Description
-----------

This program generates some geometry and a rocky looking texture. The geometry
distance field is generated with OpenCL and then is turned into a mesh using
the marching tetrahedrons algorithm on the CPU. The texture is generated with
OpenCL as well. The source is written in a subset of C++11 (gcc 4.6.1).

This was really just an excuse to do something with OpenCL.



Acknowledgements
================

I used some bits of other people's software. The copyright for each is 
included below.



webgl-noise
-----------
https://github.com/ashima/webgl-noise

I used most of the classic noise function in shaders/noise.glsl. See
LICENSE-webgl-noise.txt.


Bitstream Vera Font
-------------------
http://www-old.gnome.org/fonts/

Vera.ttf is included in the source. See LICENSE-vera.txt.


stb_truetype
------------
http://nothings.org/stb/stb_truetype.h

Sean Barrett made this public domain, and it's really handy. It's used
here mostly as-is with some minor edits to get rid of warnings.

