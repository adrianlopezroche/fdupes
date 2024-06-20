#ifndef FFMPEG_AUDIOHASH_H
#define FFMPEG_AUDIOHASH_H

#include <stdint.h>

typedef struct {
    char*  artist;
    char*  album;
    char*  title;
    char*  genre;
} ffmpeg_meta_t;

#define FFMPEG_AUDIO_HASH_MAXLEN  67

typedef struct {
    char  audiohash[FFMPEG_AUDIO_HASH_MAXLEN+1];
    uint32_t  hash;
    ffmpeg_meta_t  meta;
} ffmpeg_t;

ffmpeg_t*  ffmpeg_alloc();
void       ffmpeg_free(ffmpeg_t* obj_);

// 0 - succes
// AVERROR_INVALIDDATA - not an audio file
// other - std errno
#define FFMPEG_OK  0
#define FFMPEG_NOT_AUDIO  1
int  ffmpeg_audioinfo(ffmpeg_t* info_, const char* file_, char** err_);

uint32_t  djbhash(const char* str_);

#endif
