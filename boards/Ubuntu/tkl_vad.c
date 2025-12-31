/**
 * @file tkl_vad.c
 * @brief Voice Activity Detection implementation for Ubuntu/Raspberry Pi
 * @version 1.0
 * @date 2025-12-08
 * 
 * This implementation uses energy-based VAD with adaptive threshold.
 * Audio format: 16kHz, 16-bit, mono PCM
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
************************ TKL VAD Types *********************
***********************************************************/
typedef struct {
    uint32_t sample_rate;
    uint8_t  channel_num;
    int      speech_min_ms;
    int      noise_min_ms;
    int      frame_duration_ms;
    float    scale;
} TKL_VAD_CONFIG_T;

typedef uint8_t TKL_VAD_STATUS_T;
#define TKL_VAD_STATUS_NONE   0
#define TKL_VAD_STATUS_SPEECH 1

/***********************************************************
************************macro define************************
***********************************************************/
#define VAD_FRAME_SIZE_MS       10      // Frame size in milliseconds
#define VAD_SAMPLE_RATE         16000   // Sample rate in Hz
#define VAD_SAMPLES_PER_FRAME   (VAD_SAMPLE_RATE * VAD_FRAME_SIZE_MS / 1000)  // 160 samples

// Energy thresholds - adjust these to reduce false positives
#define VAD_ENERGY_THRESHOLD_LOW    50      // Minimum energy threshold (increase to reduce false triggers)最小能量阈值(提高以减少误触发)
#define VAD_ENERGY_THRESHOLD_HIGH   300     // High energy threshold高能量阈值
#define VAD_ADAPTIVE_ALPHA          0.98f   // Smoothing factor for noise estimation平滑因子用于噪声估计

// Debounce parameters (in frames)
#define VAD_SPEECH_HANGOVER_FRAMES  50      // Continue speech state for this many frames after energy drops (500ms)在能量下降后继续语音状态的帧数(500ms)
#define VAD_SPEECH_START_FRAMES     3       // Need this many consecutive high-energy frames to start speech (increase to reduce false triggers)需要这么多连续的高能量帧才能开始语音(提高以减少误触发)

// Debug: print energy every N frames
#define VAD_DEBUG_PRINT_INTERVAL    500     // Print energy info every 500 frames (5 seconds)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    // Configuration
    uint32_t sample_rate;
    uint8_t  channel_num;
    int      speech_min_ms;
    int      noise_min_ms;
    int      frame_duration_ms;
    float    scale;
    
    // State
    bool is_initialized;
    bool is_running;
    TKL_VAD_STATUS_T current_status;
    
    // Energy detection
    float noise_floor;              // Estimated background noise level
    float speech_threshold;         // Dynamic speech threshold
    uint32_t speech_frame_count;    // Consecutive frames with speech
    uint32_t silence_frame_count;   // Consecutive frames with silence
    uint32_t hangover_count;        // Hangover counter for smooth transitions
    
    // Frame buffer for partial data
    int16_t frame_buffer[VAD_SAMPLES_PER_FRAME];
    uint32_t frame_buffer_pos;
    
} VAD_CONTEXT_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static VAD_CONTEXT_T sg_vad_ctx = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Calculate RMS energy of audio frame
 * @param data Audio samples (16-bit signed)
 * @param samples Number of samples
 * @return RMS energy value
 */
static float __calculate_rms_energy(const int16_t *data, uint32_t samples)
{
    if (samples == 0 || data == NULL) {
        return 0.0f;
    }
    
    double sum = 0.0;
    for (uint32_t i = 0; i < samples; i++) {
        double sample = (double)data[i];
        sum += sample * sample;
    }
    
    return (float)sqrt(sum / samples);
}

/**
 * @brief Update noise floor estimation (adaptive)
 * @param energy Current frame energy
 */
static void __update_noise_floor(float energy)
{
    // Only update noise floor when we think it's silence
    if (sg_vad_ctx.current_status == TKL_VAD_STATUS_NONE && 
        energy < sg_vad_ctx.speech_threshold) {
        // Exponential moving average for noise estimation
        sg_vad_ctx.noise_floor = VAD_ADAPTIVE_ALPHA * sg_vad_ctx.noise_floor + 
                                  (1.0f - VAD_ADAPTIVE_ALPHA) * energy;
        
        // Update speech threshold based on noise floor
        sg_vad_ctx.speech_threshold = sg_vad_ctx.noise_floor * 2.5f;
        
        // Clamp to reasonable range
        if (sg_vad_ctx.speech_threshold < VAD_ENERGY_THRESHOLD_LOW) {
            sg_vad_ctx.speech_threshold = VAD_ENERGY_THRESHOLD_LOW;
        }
        if (sg_vad_ctx.speech_threshold > VAD_ENERGY_THRESHOLD_HIGH) {
            sg_vad_ctx.speech_threshold = VAD_ENERGY_THRESHOLD_HIGH;
        }
    }
}

// Frame counter for debug output
static uint32_t sg_frame_count = 0;

/**
 * @brief Process one frame of audio data
 * @param data Audio samples (16-bit signed)
 * @param samples Number of samples (should be VAD_SAMPLES_PER_FRAME)
 */
static void __process_frame(const int16_t *data, uint32_t samples)
{
    // Calculate energy
    float energy = __calculate_rms_energy(data, samples);
    
    // Apply scale factor from config
    energy *= sg_vad_ctx.scale;
    
    // Increment frame counter
    sg_frame_count++;
    
    // Debug: periodically print energy info
    if (sg_frame_count % VAD_DEBUG_PRINT_INTERVAL == 0) {
        PR_NOTICE("VAD: energy=%.1f, threshold=%.1f, noise=%.1f, status=%s",
                  energy, sg_vad_ctx.speech_threshold, sg_vad_ctx.noise_floor,
                  sg_vad_ctx.current_status == TKL_VAD_STATUS_SPEECH ? "SPEECH" : "NONE");
    }
    
    // Determine if current frame has speech
    bool is_speech = (energy > sg_vad_ctx.speech_threshold);
    
    // State machine with hysteresis
    if (sg_vad_ctx.current_status == TKL_VAD_STATUS_NONE) {
        // Currently in silence state
        if (is_speech) {
            sg_vad_ctx.speech_frame_count++;
            sg_vad_ctx.silence_frame_count = 0;
            
            // Need consecutive speech frames to transition
            if (sg_vad_ctx.speech_frame_count >= VAD_SPEECH_START_FRAMES) {
                sg_vad_ctx.current_status = TKL_VAD_STATUS_SPEECH;
                sg_vad_ctx.hangover_count = VAD_SPEECH_HANGOVER_FRAMES;
                PR_NOTICE("VAD: >>> Speech STARTED (energy=%.1f, threshold=%.1f)", 
                         energy, sg_vad_ctx.speech_threshold);
            }
        } else {
            sg_vad_ctx.speech_frame_count = 0;
            sg_vad_ctx.silence_frame_count++;
            
            // Update noise floor during confirmed silence
            __update_noise_floor(energy);
        }
    } else {
        // Currently in speech state
        if (is_speech) {
            sg_vad_ctx.speech_frame_count++;
            sg_vad_ctx.silence_frame_count = 0;
            sg_vad_ctx.hangover_count = VAD_SPEECH_HANGOVER_FRAMES;
        } else {
            sg_vad_ctx.speech_frame_count = 0;
            sg_vad_ctx.silence_frame_count++;
            
            // Use hangover to avoid premature cutoff
            if (sg_vad_ctx.hangover_count > 0) {
                sg_vad_ctx.hangover_count--;
            } else {
                // Transition to silence
                sg_vad_ctx.current_status = TKL_VAD_STATUS_NONE;
                PR_NOTICE("VAD: <<< Speech ENDED (silence_frames=%u)", 
                         sg_vad_ctx.silence_frame_count);
            }
        }
    }
}

/**
 * @brief Initialize VAD module
 * @param config VAD configuration
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_vad_init(TKL_VAD_CONFIG_T *config)
{
    PR_NOTICE(">>> tkl_vad_init called <<<");
    
    if (config == NULL) {
        PR_ERR("VAD init: config is NULL");
        return OPRT_INVALID_PARM;
    }
    
    memset(&sg_vad_ctx, 0, sizeof(VAD_CONTEXT_T));
    
    // Store configuration
    sg_vad_ctx.sample_rate = config->sample_rate;
    sg_vad_ctx.channel_num = config->channel_num;
    sg_vad_ctx.speech_min_ms = config->speech_min_ms;
    sg_vad_ctx.noise_min_ms = config->noise_min_ms;
    sg_vad_ctx.frame_duration_ms = config->frame_duration_ms;
    sg_vad_ctx.scale = (config->scale > 0) ? config->scale : 1.0f;
    
    // Initialize state
    sg_vad_ctx.is_initialized = true;
    sg_vad_ctx.is_running = false;
    sg_vad_ctx.current_status = TKL_VAD_STATUS_NONE;
    
    // Initialize energy detection parameters
    sg_vad_ctx.noise_floor = VAD_ENERGY_THRESHOLD_LOW;
    sg_vad_ctx.speech_threshold = VAD_ENERGY_THRESHOLD_LOW * 2.0f;
    sg_vad_ctx.speech_frame_count = 0;
    sg_vad_ctx.silence_frame_count = 0;
    sg_vad_ctx.hangover_count = 0;
    sg_vad_ctx.frame_buffer_pos = 0;
    
    PR_NOTICE("VAD initialized: sample_rate=%u, scale=%.2f, threshold=%.1f", 
              sg_vad_ctx.sample_rate, sg_vad_ctx.scale, sg_vad_ctx.speech_threshold);
    
    return OPRT_OK;
}

/**
 * @brief Feed audio data to VAD
 * @param data Audio data (16-bit PCM)
 * @param len Data length in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_vad_feed(uint8_t *data, uint32_t len)
{
    if (!sg_vad_ctx.is_initialized) {
        return OPRT_NOT_SUPPORTED;
    }
    
    if (!sg_vad_ctx.is_running) {
        return OPRT_OK;  // VAD not running, ignore data
    }
    
    if (data == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }
    
    // Convert to samples
    int16_t *samples = (int16_t *)data;
    uint32_t sample_count = len / sizeof(int16_t);
    uint32_t pos = 0;
    
    while (pos < sample_count) {
        // Fill frame buffer
        uint32_t to_copy = VAD_SAMPLES_PER_FRAME - sg_vad_ctx.frame_buffer_pos;
        if (to_copy > sample_count - pos) {
            to_copy = sample_count - pos;
        }
        
        memcpy(&sg_vad_ctx.frame_buffer[sg_vad_ctx.frame_buffer_pos], 
               &samples[pos], 
               to_copy * sizeof(int16_t));
        
        sg_vad_ctx.frame_buffer_pos += to_copy;
        pos += to_copy;
        
        // Process complete frame
        if (sg_vad_ctx.frame_buffer_pos >= VAD_SAMPLES_PER_FRAME) {
            __process_frame(sg_vad_ctx.frame_buffer, VAD_SAMPLES_PER_FRAME);
            sg_vad_ctx.frame_buffer_pos = 0;
        }
    }
    
    return OPRT_OK;
}

/**
 * @brief Get current VAD status
 * @return TKL_VAD_STATUS_SPEECH if speech detected, TKL_VAD_STATUS_NONE otherwise
 */
TKL_VAD_STATUS_T tkl_vad_get_status(void)
{
    if (!sg_vad_ctx.is_initialized || !sg_vad_ctx.is_running) {
        return TKL_VAD_STATUS_NONE;
    }
    
    return sg_vad_ctx.current_status;
}

/**
 * @brief Start VAD processing
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_vad_start(void)
{
    if (!sg_vad_ctx.is_initialized) {
        return OPRT_NOT_SUPPORTED;
    }
    
    // If already running, don't reset state (called every frame by audio input)
    if (sg_vad_ctx.is_running) {
        return OPRT_OK;
    }
    
    // Reset state only on first start
    sg_vad_ctx.current_status = TKL_VAD_STATUS_NONE;
    sg_vad_ctx.speech_frame_count = 0;
    sg_vad_ctx.silence_frame_count = 0;
    sg_vad_ctx.hangover_count = 0;
    sg_vad_ctx.frame_buffer_pos = 0;
    sg_vad_ctx.is_running = true;
    sg_frame_count = 0;  // Reset debug frame counter
    
    PR_NOTICE("VAD started: threshold=%.1f, noise_floor=%.1f", 
              sg_vad_ctx.speech_threshold, sg_vad_ctx.noise_floor);
    
    return OPRT_OK;
}

/**
 * @brief Stop VAD processing
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_vad_stop(void)
{
    if (!sg_vad_ctx.is_initialized) {
        return OPRT_NOT_SUPPORTED;
    }
    
    sg_vad_ctx.is_running = false;
    sg_vad_ctx.current_status = TKL_VAD_STATUS_NONE;
    
    PR_NOTICE("VAD stopped");
    
    return OPRT_OK;
}

/**
 * @brief Deinitialize VAD module
 * @return OPRT_OK on success
 */
OPERATE_RET tkl_vad_deinit(void)
{
    if (!sg_vad_ctx.is_initialized) {
        return OPRT_OK;
    }
    
    memset(&sg_vad_ctx, 0, sizeof(VAD_CONTEXT_T));
    
    PR_NOTICE("VAD deinitialized");
    
    return OPRT_OK;
}

