#include "fft.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "FFT_PROCESSOR";

// Create FFT Result
fft_result_t *fft_result_create(int length) {
  fft_result_t *result = malloc(sizeof(fft_result_t));
  if (!result) return NULL;

  result->length = length;
  result->magnitudes = malloc(length * sizeof(float));
  if (!result->magnitudes) {
    free(result);
    return NULL;
  }

  return result;
}

// Free FFT Result
void fft_result_free(fft_result_t *result) {
  if (result) {
    if (result->magnitudes) {
      free(result->magnitudes);
    }
    free(result);
  }
}

// Helper function to generate Hanning window
static void generate_hanning_window(float *window, int size) {
  float sum_sq = 0.0f;
  for (int i = 0; i < size; i++) {
    window[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (size - 1)));
    sum_sq += window[i] * window[i];
  }

  // Normalize for power conservation (RMS compensation)
  float rms_compensation = sqrtf((float)size / sum_sq);
  for (int i = 0; i < size; i++) {
    window[i] *= rms_compensation;
  }
}

// Helper function to generate Hamming window
static void generate_hamming_window(float *window, int size) {
  for (int i = 0; i < size; i++) {
    window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (size - 1));
  }
}

// Initialize FFT Processor
esp_err_t fft_processor_init(fft_processor_t *processor, int buffer_size,
                             fft_window_type_t window_type) {
  if (!processor || buffer_size <= 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Check if buffer size is power of 2
  if ((buffer_size & (buffer_size - 1)) != 0) {
    ESP_LOGE(TAG, "Buffer size must be power of 2");
    return ESP_ERR_INVALID_ARG;
  }

  processor->buffer_size = buffer_size;
  processor->buffer_half_size = buffer_size / 2;
  processor->window_type = window_type;
  processor->zero_db_reference = 1.0f;  // Standard reference level
  processor->in_exponent =
      -15;  // Standard input exponent for int16 as per documentation
  processor->initialized = false;

  // Allocate memory for buffers
  processor->window = malloc(buffer_size * sizeof(float));
  processor->windowed_buffer = malloc(buffer_size * sizeof(int16_t));
  processor->fft_buffer = malloc(buffer_size * sizeof(int16_t));
  processor->output_buffer = malloc(buffer_size * sizeof(float));

  if (!processor->window || !processor->windowed_buffer ||
      !processor->fft_buffer || !processor->output_buffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for FFT buffers");
    return ESP_ERR_NO_MEM;
  }

  // Initialize window function
  for (int i = 0; i < buffer_size; i++) {
    processor->window[i] = 1.0f;  // Default to no window
  }

  switch (window_type) {
    case FFT_WINDOW_HANNING:
      generate_hanning_window(processor->window, buffer_size);
      break;
    case FFT_WINDOW_HAMMING:
      generate_hamming_window(processor->window, buffer_size);
      break;
    case FFT_WINDOW_NONE:
    default:
      // Already initialized to 1.0f
      break;
  }

  // Initialize dl_fft handle for real FFT
  processor->fft_handle = dl_rfft_s16_init(buffer_size, MALLOC_CAP_8BIT);
  if (!processor->fft_handle) {
    ESP_LOGE(TAG, "Failed to initialize dl_fft handle");
    return ESP_ERR_NO_MEM;
  }

  processor->initialized = true;
  ESP_LOGI(TAG,
           "FFT processor initialized with buffer size: %d, window type: %d",
           buffer_size, window_type);

  return ESP_OK;
}

// Deinitialize FFT Processor
void fft_processor_deinit(fft_processor_t *processor) {
  if (!processor) return;

  if (processor->window) {
    free(processor->window);
    processor->window = NULL;
  }

  if (processor->windowed_buffer) {
    free(processor->windowed_buffer);
    processor->windowed_buffer = NULL;
  }

  if (processor->fft_buffer) {
    free(processor->fft_buffer);
    processor->fft_buffer = NULL;
  }

  if (processor->output_buffer) {
    free(processor->output_buffer);
    processor->output_buffer = NULL;
  }

  // Deinitialize dl_fft handle
  if (processor->fft_handle) {
    dl_rfft_s16_deinit(processor->fft_handle);
    processor->fft_handle = NULL;
  }

  processor->initialized = false;
  ESP_LOGI(TAG, "FFT processor deinitialized");
}

// Helper function to get magnitude index for frequency
static int get_magnitude_index_for_frequency(float frequency, float sample_rate,
                                             int fft_size) {
  float nyquist = sample_rate / 2.0f;
  return (int)(fft_size * frequency / nyquist);
}

// Process FFT with int16 input
fft_result_t *fft_processor_process(fft_processor_t *processor,
                                    const int16_t *input_buffer) {
  if (!processor || !processor->initialized || !input_buffer) {
    ESP_LOGE(TAG, "Invalid processor or input buffer");
    return NULL;
  }

  // Apply window function and convert to int16
  for (int i = 0; i < processor->buffer_size; i++) {
    float windowed_value = (float)input_buffer[i] * processor->window[i];
    // Clamp to int16 range
    if (windowed_value > 32767.0f) {
      processor->windowed_buffer[i] = 32767;
    } else if (windowed_value < -32768.0f) {
      processor->windowed_buffer[i] = -32768;
    } else {
      processor->windowed_buffer[i] = (int16_t)windowed_value;
    }
  }

  // Copy windowed data to FFT buffer
  memcpy(processor->fft_buffer, processor->windowed_buffer,
         processor->buffer_size * sizeof(int16_t));

  // Perform real FFT using dl_fft
  dl_rfft_s16_hp_run(processor->fft_handle, processor->fft_buffer,
                     processor->in_exponent, &processor->fft_exponent);

  // Convert int16 FFT output to float
  dl_short_to_float(processor->fft_buffer, processor->buffer_size,
                    processor->fft_exponent, processor->output_buffer);

  // Create result structure
  fft_result_t *result = fft_result_create(processor->buffer_half_size);
  if (!result) {
    ESP_LOGE(TAG, "Failed to create FFT result");
    return NULL;
  }

  // Calculate magnitudes and convert to dB
  // For real FFT, the output is organized as: [DC, Nyquist, Re1, Im1, Re2, Im2,
  // ...] According to dl_rfft.h documentation: x[0] = DC component (real), x[1]
  // = Nyquist component (real) x[2] = real part of 1st component, x[3] =
  // imaginary part of 1st component x[4] = real part of 2nd component, x[5] =
  // imaginary part of 2nd component, etc.
  for (int i = 0; i < processor->buffer_half_size; i++) {
    float real, imag, magnitude;

    if (i == 0) {
      // DC component (real only)
      real = processor->output_buffer[0];
      imag = 0.0f;
    } else if (i == processor->buffer_half_size - 1 &&
               processor->buffer_size % 2 == 0) {
      // Nyquist frequency (real only for even buffer sizes)
      real = processor->output_buffer[1];
      imag = 0.0f;
    } else {
      // Regular complex components
      // According to dl_rfft format: x[2+2*(i-1)] = real, x[3+2*(i-1)] = imag
      real = processor->output_buffer[2 + 2 * (i - 1)];
      imag = processor->output_buffer[3 + 2 * (i - 1)];
    }

    magnitude = sqrtf(real * real + imag * imag);

    // Convert to dB
    if (magnitude > 0.0f) {
      result->magnitudes[i] =
          20.0f * log10f(magnitude / processor->zero_db_reference);
    } else {
      result->magnitudes[i] = -INFINITY;
    }
  }

  return result;
}

// Compute frequency bands
fft_compute_bands_result_t *fft_result_compute_bands(
    const fft_result_t *fft_result, float min_frequency, float max_frequency,
    int bands_count, float sample_rate) {
  if (!fft_result || bands_count <= 0 || sample_rate <= 0) {
    ESP_LOGE(TAG, "Invalid parameters for compute bands");
    return NULL;
  }

  float nyquist_frequency = sample_rate / 2.0f;
  float actual_max_frequency = fminf(nyquist_frequency, max_frequency);

  // Allocate result structure
  fft_compute_bands_result_t *result =
      malloc(sizeof(fft_compute_bands_result_t));
  if (!result) return NULL;

  result->count = bands_count;
  result->magnitudes = malloc(bands_count * sizeof(float));
  result->frequencies = malloc(bands_count * sizeof(float));

  if (!result->magnitudes || !result->frequencies) {
    if (result->magnitudes) free(result->magnitudes);
    if (result->frequencies) free(result->frequencies);
    free(result);
    return NULL;
  }

  // Initialize arrays
  for (int i = 0; i < bands_count; i++) {
    result->magnitudes[i] = 0.0f;
    result->frequencies[i] = 0.0f;
  }

  int mag_lower_range = get_magnitude_index_for_frequency(
      min_frequency, sample_rate, fft_result->length);
  int mag_upper_range = get_magnitude_index_for_frequency(
      actual_max_frequency, sample_rate, fft_result->length);
  float ratio = (float)(mag_upper_range - mag_lower_range) / (float)bands_count;

  for (int i = 0; i < bands_count; i++) {
    int mags_start_idx = (int)floorf((float)i * ratio) + mag_lower_range;
    int mags_end_idx = (int)floorf((float)(i + 1) * ratio) + mag_lower_range;

    int count = mags_end_idx - mags_start_idx;
    if (count > 0) {
      // Calculate average magnitude for this band (convert to linear, average,
      // convert back to dB)
      float sum_linear = 0.0f;
      int valid_count = 0;
      for (int j = mags_start_idx; j < mags_end_idx && j < fft_result->length;
           j++) {
        if (fft_result->magnitudes[j] > -INFINITY) {
          // Convert dB to linear power: power = 10^(dB/10)
          float linear_magnitude =
              powf(10.0f, fft_result->magnitudes[j] / 20.0f);
          sum_linear +=
              linear_magnitude * linear_magnitude;  // Power = magnitude^2
          valid_count++;
        }
      }
      if (valid_count > 0) {
        float avg_power = sum_linear / (float)valid_count;
        float avg_magnitude = sqrtf(avg_power);
        result->magnitudes[i] =
            20.0f * log10f(avg_magnitude);  // Use standard 0dB reference
      } else {
        result->magnitudes[i] = -INFINITY;
      }
    } else if (mags_start_idx < fft_result->length) {
      result->magnitudes[i] = fft_result->magnitudes[mags_start_idx];
    }

    // Compute average frequency for this band
    float bandwidth = nyquist_frequency / (float)fft_result->length;
    result->frequencies[i] =
        (bandwidth * (float)mags_start_idx + bandwidth * (float)mags_end_idx) /
        2.0f;
  }

  return result;
}

// Free compute bands result
void fft_compute_bands_result_free(fft_compute_bands_result_t *result) {
  if (result) {
    if (result->magnitudes) free(result->magnitudes);
    if (result->frequencies) free(result->frequencies);
    free(result);
  }
}
