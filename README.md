# CCTV Dashboard

* A web dashboard used to preview live images from CCTV cameras. It mostly
use [camera-server](https://github.com/alex-lt-kong/camera-server) as its
backend and communicate with it with shared memory.

<img src="./assets/system-diagram.drawio.png" />

## Gallery

<p float="left">
    <img src="./assets/desktop.png" width="674px" alt="Desktop GUI" />    
    <img src="./assets/mobile.png" width="142px" alt="Mobile GUI" />
</p>


## Dependencies
* [Crow HTTP library](https://github.com/CrowCpp/Crow)
* `nlohmann-json` for JSON support: `apt install nlohmann-json3-dev`

## Build

* Back-end: just `make` it
* Front-end: `node babelify.js [--dev|--prod]`