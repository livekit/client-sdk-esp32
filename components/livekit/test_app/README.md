# Test App

Application for running on-device unit tests to verify correct behavior and
detect memory leaks.

## Requirements

- IDF installation with _pytest_ available (install IDF using [EIM](https://docs.espressif.com/projects/idf-im-ui/en/latest/))
- Suitable development board connected via USB
    - ESP32-S3: [ESP32-S3-BOX-3](https://www.espressif.com/en/news/ESP32-S3-BOX-3)
    - ESP32-P4: [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html)

## Build & Run

```sh
idf.py set-target esp32[s3|p4]
idf.py build
pytest
```

_pytest_ will flash the test application and run all tests configured in the test suite (see [_test_main.py_](./test_main.py)).
