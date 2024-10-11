# audacious-plugin-rpc
A Discord Rich Presence plugin for the Audacious music player!  
Fork of https://github.com/darktohka/audacious-plugin-rpc

![Preview](./assets/preview.png)

## Usage
1. Download the current release from the [releases page](https://github.com/Kenny-Hui/audacious-plugin-rpc/releases).
2. Extract `libaudacious-plugin-rpc.so` into the folder `/usr/lib/audacious/General/`.
3. Open Audacious, go to Settings > Plugins and enable the `Discord RPC` plugin.

## Changes/Features
- "Listening To" status
- Display current Album and Album Art (Fetched from Musicbrainz)
- View Album button (Link to Musicbrainz album)
- Time bar
- **Some settings like Album Art are off by default, please configure in Plugin Settings before use**

## Installation
1. Download the zip of the latest release in [Release](https://github.com/Kenny-Hui/audacious-plugin-rpc/releases) page.
2. Extract the zip, and put `libaudacious-plugin-rpc.so` to /usr/lib64/audacious/General

## Compilation
1. Clone the repository.
2. Compile and install the plugin:
```
mkdir build
cd build
cmake ..
make install
```
