# On-demand cctv server

The project is used in the following scenario:

* Twenty IP cameras are set and are constantly streaming via the RTSP protocol.
* Users need to watch real-time live streaming on an infrequent basis
(no more than a few times a day and no more than a few minutes each session).
* Users usually use average cellular mobile internet connection with usage billing
(i.e., they don't want to be charged a lot due to this service).
* Users are impatient, they don't want to wait for too long (5+ sec) to load all the live images.
* Users don't care FPS too much--0.5 frame per second is considered more than enough.

## Dependencies
* [Onion HTTP library](https://github.com/davidmoreno/onion)
    * Onion will be `make install`ed to `/usr/local/lib/`, add the directory to `$LD_LIBRARY_PATH`:
 `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/`
* Library search support: `apt install pkg-config`.
* SSL/TLS support: `apt install gnutls-dev libgcrypt-dev`
* JSON support: `apt install libjson-c-dev`.
* Video device manipulation support: `apt-get install ffmpeg`
* Image manipulation: `apt-get install libopencv-dev`
