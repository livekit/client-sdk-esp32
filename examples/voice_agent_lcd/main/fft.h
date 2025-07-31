#ifndef FFT_H
#define FFT_H

#include "dl_fft.h"
#include "dl_rfft.h"
#include "esp_log.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FFT Processor Library for ESP32
 *
 * This library implements FFT processing using dl_fft's int16 real FFT
 * functions. It provides real-to-complex FFT processing optimized for audio
 * analysis with 16-bit integer input.
 *
 * Key features:
 * - Uses dl_fft's int16 real FFT functions for efficiency
 * - Supports Hanning and Hamming windowing
 * - Converts FFT results to magnitude spectrum in dB
 * - Provides frequency band computation
 * - Handles int16 to float conversion internally
 */

// FFT Window Types
typedef enum {
  FFT_WINDOW_NONE = 0,
  FFT_WINDOW_HANNING,
  FFT_WINDOW_HAMMING
} fft_window_type_t;

// FFT Result structure
typedef struct {
  int length;
  float *magnitudes;
} fft_result_t;

// FFT Compute Bands Result structure
typedef struct {
  int count;
  float *magnitudes;
  float *frequencies;
} fft_compute_bands_result_t;

// FFT Processor structure for dl_fft int16 real FFT implementation
typedef struct {
  int buffer_size;       // Must be power of 2 for dl_fft
  int buffer_half_size;  // buffer_size / 2
  fft_window_type_t window_type;

  float *window;             // Window function coefficients [buffer_size]
  int16_t *windowed_buffer;  // Windowed int16 input data [buffer_size]
  int16_t *fft_buffer;       // Int16 FFT data buffer [buffer_size]
  float *output_buffer;      // Float output after conversion [buffer_size]
  float zero_db_reference;   // Reference level for dB conversion

  // dl_fft specific fields
  dl_fft_s16_t *fft_handle;  // dl_fft handle
  int in_exponent;           // Input exponent for scaling
  int fft_exponent;          // FFT output exponent

  bool initialized;
} fft_processor_t;

/**
 * @brief Create FFT result structure
 * @param length Number of magnitude values (typically buffer_size/2)
 * @return Pointer to allocated fft_result_t or NULL on failure
 */
fft_result_t *fft_result_create(int length);

/**
 * @brief Free FFT result structure
 * @param result Pointer to fft_result_t to free
 */
void fft_result_free(fft_result_t *result);

/**
 * @brief Initialize FFT processor using dl_fft int16 real FFT
 * @param processor Pointer to fft_processor_t structure
 * @param buffer_size Size of input buffer (must be power of 2)
 * @param window_type Type of windowing function to apply
 * @return ESP_OK on success, error code on failure
 *
 * Note: This function uses dl_rfft_s16_init() internally
 */
esp_err_t fft_processor_init(fft_processor_t *processor, int buffer_size,
                             fft_window_type_t window_type);

/**
 * @brief Deinitialize FFT processor and free allocated memory
 * @param processor Pointer to fft_processor_t structure
 *
 * Note: This function calls dl_rfft_s16_deinit() internally
 */
void fft_processor_deinit(fft_processor_t *processor);

/**
 * @brief Process int16 input signal using dl_fft and return magnitude spectrum
 * @param processor Pointer to initialized fft_processor_t
 * @param input_buffer Int16 input signal [buffer_size samples]
 * @return Pointer to fft_result_t with magnitude spectrum in dB, or NULL on
 * failure
 *
 * Processing steps:
 * 1. Apply windowing function (converts to int16)
 * 2. Perform real FFT using dl_rfft_s16_hp_run()
 * 3. Convert output to float using dl_short_to_float()
 * 4. Calculate magnitude spectrum and convert to dB
 */
fft_result_t *fft_processor_process(fft_processor_t *processor,
                                    const int16_t *input_buffer);

/**
 * @brief Compute frequency bands from FFT magnitude spectrum
 * @param fft_result FFT result containing magnitude spectrum
 * @param min_frequency Minimum frequency for band analysis (Hz)
 * @param max_frequency Maximum frequency for band analysis (Hz)
 * @param bands_count Number of frequency bands to compute
 * @param sample_rate Sample rate of the original signal (Hz)
 * @return Pointer to fft_compute_bands_result_t or NULL on failure
 */
fft_compute_bands_result_t *fft_result_compute_bands(
    const fft_result_t *fft_result, float min_frequency, float max_frequency,
    int bands_count, float sample_rate);

/**
 * @brief Free frequency bands result structure
 * @param result Pointer to fft_compute_bands_result_t to free
 */
void fft_compute_bands_result_free(fft_compute_bands_result_t *result);

#ifdef __cplusplus
}
#endif

#endif  // FFT_H