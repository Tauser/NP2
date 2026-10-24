#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { SOUND_NOTIFICATION, SOUND_ALARM, SOUND_STARTUP, SOUND_CATEGORY_COUNT } sound_category_t;
#define SOUND_VARIANT_COUNT 4U
#define SOUND_SAMPLE_RATE 22050U
typedef struct { uint16_t hz, duration_ms, gap_ms; } sound_note_t;
typedef struct { const sound_note_t *notes; uint8_t note_count; uint16_t amplitude; } sound_melody_t;
typedef struct { const sound_melody_t *melody; uint32_t position, phase, phase_step; uint8_t note; } sound_player_t;
bool sound_choice_valid(sound_category_t category, uint8_t variant);
const char *sound_choice_name(sound_category_t category, uint8_t variant);
bool sound_player_start(sound_player_t *player, sound_category_t category, uint8_t variant);
/* PCM16 mono, bounded streaming without allocation, I/O or wall clock. */
size_t sound_player_read(sound_player_t *player, int16_t *out, size_t capacity);
