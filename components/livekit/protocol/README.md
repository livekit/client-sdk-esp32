# LiveKit Protocol

This component contains generated Protocol Buffer bindings for C (generated with [Nanopb](https://jpa.kapsi.fi/nanopb/docs/)) and a Python script to update them.

## Update Script

When a new protocol version is available, update the generated bindings as follows:
1. Navigate to the [*update/*](./update/) directory
2. Set the the release tag in [*version.ini*](./update/version.ini)
3. Run the update script (Nanopb must be available in your path):
```sh
python update.py
```
4. Review and commit changes.

## Generation Options

Nanopb provides a rich set of [generation options](https://jpa.kapsi.fi/nanopb/docs/reference.html#generator-options) for generating  bindings that are suitable for an embedded environment; *.options* files are placed alongside Protobuf files in the [*protobufs*](./protobufs/) directory.
