# Codec_board

## Overview
`codec_board` is a simple implementation of board support for a variety of ESP32 series boards. 
Its main purpose is to:
- Gather board-specific peripheral settings from user.
- Add drivers for the supported peripherals.
- Provide high-level APIs to interact with the peripherals or abstract the peripherals into manageable handles.

## Supported Peripherals

- **Codec**  
  Currently support `ES8311`, `ES8388`, `ES7210`, `ES7243`, user can add other codec also.  
  After `init_codec` is called, codec is abstracted into `esp_codec_dev` handle.  
  Then use can use `esp_codec_dev` API to do playback and record.  
  Simple configuration of codec as:
```
out: {codec: ES8311, pa: 38, use_mclk: 0, pa_gain:6}
in: {codec: ES7210, i2s_port: 1}
```

- **SDcard**  
  Users can configure SDCard related GPIO then use API `mount_sdcard` to mount sdcard to use it.  
  SDcard configuration as:
```
sdcard: {clk: 43, cmd: 44, d0: 39, d1: 40, d2: 41, d3: 42}
```

- **Camera**  
  Currently support DVP and MIPI camera, use can get camera settings use API `get_camera_cfg`

- **LCD**  
   Currently only support LCD on `S3_Korvo_V2` and `ESP32_P4_FUNCTION_EV`.
   User can get LCD handle through API `board_get_lcd_handle` after `board_lcd_init`

## Customized a New Board
To reuse the pre-defined devices, follow these steps:
1. Add a new section to the [board_cfg.txt](board_cfg.txt) file and modify corresponding GPIO settings as needed.
2. Call `set_codec_board_type` with the name of the newly added section.  
For new devices, you will need to manually modify the code.

## Comparison with esp-bsp

[esp-bsp](https://github.com/espressif/esp-bsp) supports almost all devices carried on board while this module only support media related devices. It provides a quick verification option if reusing the same device with different GPIO settings. Users can easily replace this module with `esp-bsp` by making the following substitutions:

- `bsp_audio_codec_speaker_init`: Replaces `get_playback_handle`
- `bsp_audio_codec_microphone_init`: Replaces `get_record_handle`
- `bsp_display_new`: Replaces `board_get_lcd_handle`