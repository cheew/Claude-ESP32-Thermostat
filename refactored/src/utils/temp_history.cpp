/**
 * temp_history.cpp
 * Temperature History Tracking Implementation
 */

#include "temp_history.h"
#include <sys/time.h>

// History buffer (circular)
static TempHistoryPoint_t history_buffer[HISTORY_BUFFER_SIZE];
static int history_count = 0;
static int history_index = 0;
static unsigned long last_sample_time = 0;
static unsigned long boot_time = 0;

void temp_history_init(unsigned long boot_time_ms) {
    boot_time = boot_time_ms;
    history_count = 0;
    history_index = 0;
    last_sample_time = 0;
    memset(history_buffer, 0, sizeof(history_buffer));
}

void temp_history_record(float temp) {
    unsigned long current_time = millis();

    // Check if enough time has passed since last sample
    if (last_sample_time > 0 && (current_time - last_sample_time) < HISTORY_SAMPLE_INTERVAL) {
        return;  // Not time to sample yet
    }

    // Get current Unix timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long timestamp = tv.tv_sec;

    // Store reading in circular buffer
    history_buffer[history_index].timestamp = timestamp;
    history_buffer[history_index].temperature = temp;

    // Update indices
    history_index = (history_index + 1) % HISTORY_BUFFER_SIZE;
    if (history_count < HISTORY_BUFFER_SIZE) {
        history_count++;
    }

    last_sample_time = current_time;
}

int temp_history_get_count(void) {
    return history_count;
}

const TempHistoryPoint_t* temp_history_get_point(int index) {
    if (index < 0 || index >= history_count) {
        return nullptr;
    }

    // Calculate actual buffer index (oldest first)
    int buffer_idx;
    if (history_count < HISTORY_BUFFER_SIZE) {
        // Buffer not full yet, start from beginning
        buffer_idx = index;
    } else {
        // Buffer full, calculate from current position
        buffer_idx = (history_index + index) % HISTORY_BUFFER_SIZE;
    }

    return &history_buffer[buffer_idx];
}

void temp_history_clear(void) {
    history_count = 0;
    history_index = 0;
    last_sample_time = 0;
    memset(history_buffer, 0, sizeof(history_buffer));
}

unsigned long temp_history_get_last_sample_time(void) {
    return last_sample_time;
}
