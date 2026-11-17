# Test App

Application for running on-device tests to verify correct behavior and
detect memory leaks. It includes unit tests and an end-to-end (E2E) test that
connects the board to a real LiveKit room.

## Requirements

- IDF installation with _pytest_ available (install IDF using [EIM](https://docs.espressif.com/projects/idf-im-ui/en/latest/))
- Suitable development board connected via USB
    - ESP32-S3: [ESP32-S3-BOX-3](https://www.espressif.com/en/news/ESP32-S3-BOX-3) (connects with Wi-Fi)
    - ESP32-P4: [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html) (connects with Ethernet)

## Setup

Do these steps once:

1. Install the Python dependencies into the IDF environment:

    ```sh
    pip install -r requirements.txt
    ```

2. Configure the E2E test:

    ```sh
    cp .env.example .env
    ```

    To use a local server, start it so that the board can reach it on your network, and include the port in the URL:

    ```sh
    livekit-server --dev --bind 0.0.0.0   # LIVEKIT_URL=ws://<your computer's IP>:7880
    ```

## Run

Build for the chip of the connected board, then run the tests:

```sh
idf.py set-target esp32s3 build
pytest --target esp32s3
```

> [!NOTE]
> The board echoes the values it receives, so the Wi-Fi password and token appear in the device logs. Git ignores the _logs_ folder.
