# DoorBell Demo

## OverView
This demo demonstrates how to use `esp_webrtc` to build a doorbell application. The code is based on the Google [apprtc](https://github.com/webrtc/apprtc) project, with custom signaling through WebSocket.

## Hardware requirement
The default setup uses the [ESP32P4-Function-Ev-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html), which includes one SC2336 camera.

## How to build

### IDF version
Can select IDF master or IDF release v5.4.

### Dependencies
This demo only depends on the **ESP-IDF**. All other required modules will be automatically downloaded from the [ESP-IDF Component Registry](https://components.espressif.com/).

### Change Default Settings
1. Modify the Wi-Fi SSID and password in the file in [settings.h](main/settings.h)
2. If you are using a different camera type or resolution, update the settings for the camera type and resolution in [settings.h](main/settings.h)
3. If you are using USB-JTAG to download, uncomment the following configuration in [sdkconfig.defaults.esp32p4](sdkconfig.defaults.esp32p4)
```
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
```

### Build
```
idf.py -p YOUR_SERIAL_DEVICE flash monitor
```

## Testing

After the board boots up, it will attempt to connect to the configured Wi-Fi SSID. If you want to connect to a different Wi-Fi STA, you can use the following CLI command:
```
wifi ssid psw
```

Once connected to Wi-Fi, the board will automatically try to join a room generated from its MAC address. The room name will appear in the console:
```
W (9801) Webrtc_Test: Please use browser to join in espxxxxxx on https://webrtc.espressif.com/doorbell
```
User also can use `leave` command leave the room and use the `join` command to join other random room. Make sure the room is empty before entering:
```
join random_room_id
```
If you're the first to join, you’ll see:
```
Initials set to 1
```

Then, use a Chrome/Edge browser to enter the same room at [DoorBellDemo](https://webrtc.espressif.com/doorbell).

### Interactions

1. **Open Door:**
   - In the browser, click the `Door` icon. This will send the "open door" command to the board, and the board will play the "Door is opened" tone.
   - The board will reply with "Door Opened" and the browser will display the message: `Receiving Door opened event`.

2. **Calling:**
   - Press the `Ring` button (boot key) on the board. The board will play the ring music
   - The browser will popup `Accept Call` or `Deny Call` icon while playing ring music. If accepted, a two-way voice communication and one-way video communication (board to browser) will be established between the board and browser. If denied, the call will hang up.  
   To use a different key as the Ring button, change the `DOOR_BELL_RING_BUTTON`  in [settings.h](main/settings.h).

3. **Clear-up Test:**
   - In the browser, click the `Exit` icon to leave the room.
   - On the board, enter the `leave` command to exit the room.

## Technical Details
  To support the doorbell functionality, this demo uses [apprtc](https://github.com/webrtc/apprtc) and requires separate signaling from the peer connection build logic. The peer connection is only established when a special signaling message is received from the peer.

### Key Changes in `esp_webrtc`:
- **`no_auto_reconnect` Configuration**: This configuration disables the automatic building of the peer connection when the signaling connection is established.
- **`esp_webrtc_enable_peer_connection` API**: A new API is introduced to manually control the connection and disconnection of the peer connection.

All other steps follow the typical call flow of `esp_webrtc`. For more details on the standard connection build flow, refer to the [Connection Build Flow](../../components/esp_webrtc/README.md#typical-call-sequence-of-esp_webrtc).

### QA
- If the board unexpectedly leaves the room, fail to re-enter same room.
  Server will keep the room for 1-2 minutes before timing out. The user must wait for the timeout to expire before retrying.
  Or `leave` room firstly then `join` a random room id.

- If user want to change cn server can use following command before enter `join`
   ```
   server 1
   ```

