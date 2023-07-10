# ISO-9660 Rebuilder for PS3

Library and command line tool for rebuilding PS3 disc rips into Redump-verifiable "proper" ISOs, with the help of IRD files. Use [DecVerify](https://github.com/alcamiz/DecVerify) to verify that the ISO was rebuilt correctly. Currently only supports JB Folders as input, and hasn't reached a stable release.

## Features:

- Appropriate handling of non-contiguous multi-extent files.
- Automatic IRD retrieval from Zar's [archive](http://ps3ird.free.fr).
- Automatic PUP file retrieval from Zelfie's [archive](http://archive.midnightchannel.net).

## Limitations:

- Only Linux is supported (due to argp library usage).
- Depends on system support for 64 bit offsets.
- Will never support split ISO files.

## Planned Features:

- Separate standalone libraries for handling ISO-9660/IRD/SFO formats.
- Real-time progress report on the command line.
- Decreased RAM usage when rebuilding large discs.
- Removal of endianness and alignment issues for some operations.
- Support for ISOs as input.
- Support for automatically encrypting ISOs.
- Automatically verification of output with Redump's database.
- Fallback for online archives.
- User-provided filename pattern with SFO/IRD information.
- Option for statically-compiled mbedTLS/zlib/curl

## Building:

First install development packages for mbedTLS, zlib, and libcurl. Then run the following:

```
git clone https://github.com/alcamiz/PS3-Rebuilder
cd PS3-Rebuilder
make
```

The executable will be called ps3-rebuilder. Please report any issues you may have when compiling. 

## Credits:

- Zar and Sandungas for their documentation of the IRD format.
- Estwald for certain ISO-9660 helper functions.
- PS3 Developer Wiki for their documentation of the SFO format.
- Zar and Zelfie for allowing access to their online archives.

