#include "audio_visualizer.h"
#include "esp_log.h"
#include "fft.h"
#include "media_lib_os.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <mutex>
#include <queue>
#include <vector>

static const char *TAG = "audio_visualizer";
static std::queue<std::vector<uint8_t>> audio_data_queue;
static media_lib_thread_handle_t thread;
static fft_processor_t *fft_processor = NULL;
static bool running = false;
static bool fft_processor_initialized = false;
static SemaphoreHandle_t sem;

static void fft_processor_thread(void *arg);

int audio_visualizer_init(void) {
  ESP_LOGI(TAG, "Audio visualizer initialized");

  if (!fft_processor) {
    fft_processor = (fft_processor_t *)malloc(sizeof(fft_processor_t));
    memset(fft_processor, 0, sizeof(fft_processor_t));
    // Initialize FFT processor
    esp_err_t ret = fft_processor_init(fft_processor, 1024, FFT_WINDOW_HANNING);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize FFT processor: %s",
               esp_err_to_name(ret));
    }
    fft_processor_initialized = ret == ESP_OK;

    sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
      ESP_LOGE(TAG, "Failed to create semaphore");
      return -1;
    }
    running = true;
    // Create a thread for processing audio data
    media_lib_thread_create_from_scheduler(&thread, "fft_render",
                                           fft_processor_thread, NULL);
  }
  return 0;
}

static void fft_processor_thread(void *arg) {
  while (running) {
    xSemaphoreTake(sem, pdMS_TO_TICKS(1000));
    while (!audio_data_queue.empty()) {
      auto audio_data = audio_data_queue.front();
      audio_data_queue.pop();
      if (fft_processor_initialized) {
        // Apply FFT processing
        fft_result_t *fft_result = fft_processor_process(
            fft_processor, (const int16_t *)audio_data.data());
        if (fft_result) {
          // Process FFT result if needed
          // For example, you can log or analyze the magnitudes
          ESP_LOGE(TAG, "FFT result length: %d", fft_result->length);

          fft_compute_bands_result_t *bands =
              fft_result_compute_bands(fft_result, 0, 8000, 5, 16000);

          if (bands) {
            // Process frequency bands if needed
            ESP_LOGI(TAG, "FFT bands length: %d", bands->count);
            for (int i = 0; i < bands->count; i++) {
              ESP_LOGE(TAG, "Band %d: magnitude=%.2f, frequency=%.2f", i,
                       bands->magnitudes[i], bands->frequencies[i]);
            }
            // Free the bands result after processing
            fft_compute_bands_result_free(bands);
          }

          fft_result_free(fft_result);
        } else {
          ESP_LOGE(TAG, "FFT processing failed");
        }
      }
    }
  }
}

int audio_visualizer_processing(uint8_t *audio_data, uint32_t data_size) {

  audio_data_queue.push(
      std::vector<uint8_t>(audio_data, audio_data + data_size));

  if (sem) {
    xSemaphoreGive(sem);
  } else {
    ESP_LOGE(TAG, "Semaphore not initialized");
  }

  return 0;
}

int audio_visualizer_deinit(void) {
  if (fft_processor) {
    fft_processor_deinit(fft_processor);
    fft_processor_initialized = false;
    ESP_LOGI(TAG, "FFT processor deinitialized");
  }
  // close the thread
  if (thread) {
    running = false;
    xSemaphoreGive(sem);
    media_lib_thread_destroy(thread);
    thread = NULL;
  }
  return 0;
}