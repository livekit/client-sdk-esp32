# ESP-IDF patches required for ESP32-S31

ESP-IDF v6.1.0 has a dual-core ESP32-S31 cache fault in its flash-operation
coordination. The parked core leaves branch prediction enabled while the other
core accesses flash, so a speculative fetch can trigger a layout-sensitive
`Cache access error` (`MCAUSE 0x19`). See Espressif issue
[#18948](https://github.com/espressif/esp-idf/issues/18948).

Apply the workaround after activating ESP-IDF and before building an S31
application:

```sh
S31_PATCH="$(pwd)/tools/esp-idf-patches/esp-idf-v6.1-s31-branch-predictor.patch"
git -C "$IDF_PATH" apply --check "$S31_PATCH"
git -C "$IDF_PATH" apply "$S31_PATCH"
```

In PowerShell, resolve the same patch to an absolute path and use
`$env:IDF_PATH` in place of `$IDF_PATH`.

The patch mirrors Espressif's proposed fix by disabling branch prediction on
the parked core. It is unnecessary for single-core builds and should be removed
when the fix is included in the selected ESP-IDF revision.
