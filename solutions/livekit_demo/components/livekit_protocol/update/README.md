# LiveKit Protocol Update Tool

This directory contains a simple tool to download the LiveKit protocol Protobuf definitions and
generate C-language bindings for them using [nanopb](https://github.com/nanopb/nanopb).

## Usage

From within this directory, run:
```sh
python update.py
```

This script performs three operations:
1. Downloads the protocol whose version is specified in *version.ini*.
2. Copies the required Protobuf files into the *protobuf* directory.
3. Generates C-language bindings, placing them in the correct place in the source directory.

## Generation options

Nanopb provides a rich set of [generation options](https://jpa.kapsi.fi/nanopb/docs/reference.html#generator-options) for generating  bindings that are suitable for an embedded environment; *.options* files are placed alongside Protobuf files in the *protobufs* directory.