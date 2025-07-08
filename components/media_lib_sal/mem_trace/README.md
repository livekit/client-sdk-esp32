# Memory trace for media_lib_sal

## Functions

This module is designed to keep tracing of memory status for modules using media_lib_sal.  
It supports following tracing functions:
- Support tracing for memory usage of different modules
- Support tracing for memory leakage
- Support save memory allocation and free history to file for offline analysis  
  Script [mem_trace.pl](mem_trace.pl) can draw allocation tree with detail function line information
- Support tracing for Xtensa, Risc-V, Linux architecture
- Support tracing on runtime, no overhead when tracing not enabled

## How to use

1. Call `media_lib_start_mem_trace` manually or enable trace automatically after `media_lib_sal` adapter installed.
    ```
    ADF Library Configuration --> Support trace memory automatically
    ```

2. After `MEDIA_LIB_MEM_TRACE_MODULE` enabled, memory allocated by `media_lib_module_malloc` and other similar API will be traced. Users can use `media_lib_get_mem_usage` to see module memory usage information.

3. After `MEDIA_LIB_MEM_TRACE_LEAKAGE` enabled, users can call `media_lib_print_leakage` to see module memory leakages.

4. Stop tracing after call `media_lib_stop_mem_trace`, total memory usage and leakages will be shown, memory history will save to file system if enabled.


## Use offline tool to analysis memory allocation details
1. Enable save memory usage history to file option
    ```
    ADF Library Configuration --> Support trace memory automatically --> Trace to save memory history
    ```
    After test, call `media_lib_stop_mem_trace` to sync history to files.
    Users can use either SDCard or internal flash(SPIFFS partition) to store file.

2. Show memory allocation details tree
    ```
    ./mem_trace.pl elf_file_path trace_log_path

    Example:
    ./mem_trace.pl build/play_mp3_control.elf /media/tempo/3532-3132/TRACE.LOG
    ├── root:	max-use:42949  Leak: 471
    │   ├── esp_codec:	max-use:30480
    │   │   ├── esp-mp3:	max-use:30480
    │   │   │   ├── src:	max-use:30480
    │   │   │   │   ├── pvmp3_framedecoder.cpp:	max-use:30480
    │   │   │   │   │   ├── 689:	max-use:8448
    │   │   │   │   │   ├── 700:	max-use:8192
    │   │   │   │   │   ├── 693:	max-use:4624
    │   │   │   │   │   ├── 691:	max-use:4608
    │   │   │   │   │   ├── 697:	max-use:4608
    │   ├── components:	max-use:8267  Leak: 471
    │   │   ├── audio_pipeline:	max-use:6572  Leak: 48
    │   │   │   ├── audio_element.c:	max-use:4168
    │   │   │   │   ├── 468:	max-use:3600
    │   │   │   │   ├── 943:	max-use:432
    │   │   │   │   ├── 679:	max-use:128
    │   │   │   │   ├── 553:	max-use:8
    │   │   │   ├── ringbuf.c:	max-use:2084
    │   │   │   │   ├── 68:	max-use:2048
    │   │   │   │   ├── 67:	max-use:36
    │   │   │   ├── audio_event_iface.c:	max-use:240  Leak: 48
    │   │   │   │   ├── 67:	max-use:192  Leak: 48
    │   │   │   │   ├── 185:	max-use:48
    │   │   │   ├── audio_pipeline.c:	max-use:80
    │   │   │   │   ├── 251:	max-use:32
    │   │   │   │   ├── 284:	max-use:32
    │   │   │   │   ├── 472:	max-use:16
    │   │   ├── audio_stream:	max-use:1336
    │   │   │   ├── i2s_stream.c:	max-use:1336
    │   │   │   │   ├── 142:	max-use:1200
    │   │   │   │   ├── 383:	max-use:136
    │   │   ├── esp_peripherals:	max-use:315  Leak: 315
    │   │   │   ├── lib:	max-use:140  Leak: 140
    │   │   │   │   ├── adc_button:	max-use:140  Leak: 140
    │   │   │   │   │   ├── adc_button.c:	max-use:140  Leak: 140
    │   │   │   │   │   │   ├── 105:	max-use:72  Leak: 72
    │   │   │   │   │   │   ├── 88:	max-use:28  Leak: 28
    │   │   │   │   │   │   ├── 80:	max-use:24  Leak: 24
    │   │   │   │   │   │   ├── 415:	max-use:16  Leak: 16
    │   │   │   ├── esp_peripherals.c:	max-use:111  Leak: 111
    │   │   │   │   ├── 316:	max-use:48  Leak: 48
    │   │   │   │   ├── 138:	max-use:48  Leak: 48
    │   │   │   │   ├── 320:	max-use:15  Leak: 15
    │   │   │   ├── driver:	max-use:40  Leak: 40
    │   │   │   │   ├── i2c_bus:	max-use:40  Leak: 40
    │   │   │   │   │   ├── i2c_bus.c:	max-use:40  Leak: 40
    │   │   │   │   │   │   ├── 70:	max-use:40  Leak: 40
    │   │   │   ├── periph_adc_button.c:	max-use:24  Leak: 24
    │   │   │   │   ├── 77:	max-use:24  Leak: 24
    │   │   ├── audio_hal:	max-use:100  Leak: 100
    │   │   │   ├── audio_hal.c:	max-use:72  Leak: 72
    │   │   │   │   ├── 44:	max-use:72  Leak: 72
    │   │   │   ├── audio_volume.c:	max-use:28  Leak: 28
    │   │   │   │   ├── 119:	max-use:28  Leak: 28
    │   │   ├── audio_board:	max-use:8  Leak: 8
    │   │   │   ├── esp32_s3_korvo2_v3:	max-use:8  Leak: 8
    │   │   │   │   ├── board.c:	max-use:8  Leak: 8
    │   │   │   │   │   ├── 42:	max-use:8  Leak: 8
    │   ├── esp_processing:	max-use:4266
    │   │   ├── esp-wrapper:	max-use:4246
    │   │   │   ├── mp3_decoder.c:	max-use:4246
    │   │   │   │   ├── 85:	max-use:2106
    │   │   │   │   ├── 291:	max-use:1968
    │   │   │   │   ├── 552:	max-use:88
    │   │   │   │   ├── 189:	max-use:56
    │   │   │   │   ├── 183:	max-use:28
    │   │   ├── esp-share:	max-use:20
    │   │   │   ├── mpeg_parser.c:	max-use:20
    │   │   │   │   ├── 71:	max-use:20

    ```

3. Find heap issue possible cause  
   We often meet `tlsf_assert` when `tlsf` check heap corruption happen but the assertion point may far away from the causes. Following section give a way to find the possible causes using offline search skills.  
   Here is the test example:
    ```c
    int test_used_after_free() {
        uint8_t* data = (uint8_t*) media_lib_malloc(1024);   // line 563
        uint8_t* data1  = (uint8_t*) media_lib_malloc(1024); // line 564
        media_lib_free(data);
        // data is used after free and overwrote
        memset(data, 0xFF, 1028);
        media_lib_free(data1);
        return 0;
    }
    ```
    Uses can enable gdb-stub to help to locate the issue point.


 * 3-1 Stop tracing before `tlsf_assert` happen
    ```c
    /* Add following code in heap/multi_heap.c
     * Need leave critical section so that write history can be finished
     */
    multi_heap_handle_t assert_heap;
    void heap_assert_unlock() {
        if (assert_heap) {
            MULTI_HEAP_UNLOCK(assert_heap->lock);
        }
    }

    void multi_heap_internal_lock(multi_heap_handle_t heap)
    {
        assert_heap = heap; // Add this line
        MULTI_HEAP_LOCK(heap->lock);
    }

    /* Add following code in heap/tlsf/tlsf_common.h
     * Let memory trace stop before assert
     */
    void heap_assert_unlock();
    void media_lib_stop_mem_trace();
    #if !defined (tlsf_assert)
    #define tlsf_assert(a) { if (a) {} else {heap_assert_unlock(); media_lib_stop_mem_trace(); *(int*)0 = 0;}} 
    #endif
    ```

 * 3-2 When `tlsf_assert` happen, use gdb to check corruption address    
  It can see that block header before memory is wrong and most possibly caused by memory overwritten.  
    ```
    0x40381899 in tlsf_free (tlsf=0x3c0a0014, ptr=0x3c0b6770) at /home/tempo/c6/esp-adf-internal/esp-idf/components/heap/tlsf/tlsf.c:1119
    1119			tlsf_assert(!block_is_free(block) && "block already marked as free");
    (gdb) p block
    $1 = (block_header_t *) 0x3c0b6768
    (gdb) p *block
    $2 = {prev_phys_block = 0xffffffff, size = 4294967295, next_free = 0x3c0a0014, prev_free = 0x3c0a0014}
    ```

* 3-3 Use [mem_trace.pl](mem_trace.pl) to find possible causes  
  Use the address before header to check where the address come from then review code to find deeper causes.
    ```
    $ mem_trace.pl play_mp3_control.elf trace.log --last_malloc 0x3c0b6768
    Get last malloc buffer: 3c0b636c position:1020/1024 freed:1
    /home/tempo/c6/esp-adf-internal/components/esp-adf-libs/media_lib_sal/media_lib_os.c:76
    /home/tempo/c6/esp-adf-internal/examples/get-started/play_mp3_control/main/malloc_test.c:563
    /home/tempo/c6/esp-adf-internal/examples/get-started/play_mp3_control/main/malloc_test.c:574
    ```

## How to save data into flash and read from it

* Add spiffs partition to partition table
    ```
    factory,    0,    0,        0x10000, 1M
    storage,    data, spiffs,   ,        256k
    ```

* Mount spiffs partition in code
    ```c
    #include "esp_spiffs.h"
    static int mount_spiffs(void)
    {
        esp_vfs_spiffs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
        };
        esp_vfs_spiffs_register(&conf);
        return 0;
    }
    ```

* Change memory history save path
    ```
    ADF Library Configuration --> Support trace memory automatically --> Memory trace save path
    ```

* After history saved, dump it from flash
    ```
    1: Get partition data information, memorize spiffs partition offset and size
      $ idf.py partition-table
        storage,data,spiffs,0x110000,256K,

    2: Load partition data to PC use esptool
      $ esptool.py --chip esp32s3 -p /dev/ttyUSB0 -b 460800 --before=default_reset --after=hard_reset read_flash  0x110000 0x40000 spiffs.bin

    3: Unpack partition data use mkspiffs:
      $ git clone https://github.com/igrr/mkspiffs.git
      $ cd mkspiff
      $ ./build_all_configs.sh
      $ mkspiffs -u my_spiffs  spiffs.bin

    Afterwards users can get the history file under folder my_spiffs.
    If mkspiffs installed into system path, you can use script to finish step 1-3 automatically.
      $ ./mem_trace.pl --load_spiffs
    ```

* Analysis history file using [mem_trace.pl](mem_trace.pl)
    ```
    mem_trace.pl build/play_mp3_control.elf  my_spiffs/trace.log
    ```

Notes: If SPI-RAM is enabled, write thread stack is put to SPI-RAM, you need enable `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` and `CONFIG_SPIRAM_RODATA` to avoid cache assertion.

   