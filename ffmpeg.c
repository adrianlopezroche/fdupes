/* Copyright (c) 2023 Ray whatdoineed2do @ gmail com
 *
   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   gcc -g -DHAVE_FFMPEG -DFFMPEG_UNIT_TEST \
     $( PKG_CONFIG_PATH=/usr/local/lib64/ffmpeg6/pkgconfig pkg-config -cflags -libs libavutil libavcodec libavformat) 
     ./ffmpeg.c
 */

#include "ffmpeg.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>

#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/hash.h>
#include <libavutil/log.h>

static char  errbuf[64] = { 0 };

static inline char * err2str (int errnum)
{
    av_strerror (errnum, errbuf, sizeof (errbuf));
    return errbuf;
}

static void  _avlog_callback_null(void *ptr, int level, const char *fmt, va_list vl)
{ }

static bool _init = false;


static char* const  na = "<n/a>";

ffmpeg_t*  ffmpeg_alloc()
{
    ffmpeg_t*  obj = malloc(sizeof(ffmpeg_t));
    memset(obj, 0, sizeof(ffmpeg_t));

    obj->meta.artist = na;
    obj->meta.album = na;
    obj->meta.title = na;
    obj->meta.genre = na;

    obj->audiohash[0] = '0';

    return obj;
}

void  ffmpeg_free(ffmpeg_t* obj_)
{
    if (obj_ == NULL) {
        return;
    }

#define free_ifna(p)  { if (p != na) free(p); }
    free_ifna(obj_->meta.artist);
    free_ifna(obj_->meta.album);
    free_ifna(obj_->meta.title);
    free_ifna(obj_->meta.genre);

    free(obj_);
}


int  ffmpeg_audioinfo(ffmpeg_t* info_, const char* file_, char** err_)
{
#ifndef HAVE_FFMPEG
    return -EPERM;
#else
    if (!_init) {
	av_log_set_flags(AV_LOG_SKIP_REPEATED);
	av_log_set_callback(_avlog_callback_null);
	_init = true;
    }

    AVFormatContext *ctx = NULL;
    AVPacket *pkt = NULL;
    int64_t pktnum  = 0;
    int ret;
    int audio_stream_idx = -1;
    char  err[1024];

    struct AVHashContext *hash = NULL;

    ret = avformat_open_input(&ctx, file_, NULL, NULL);
    if (ret < 0) {
	if (ret == AVERROR_INVALIDDATA) {
	    ret = FFMPEG_NOT_AUDIO;
	    snprintf(err, sizeof(err), "%s", av_err2str(ret));
	}
	else {
	    snprintf(err, sizeof(err), "unable to open input - %s", av_err2str(ret));
	}
        *err_ = strdup(err);
        goto cleanup;
    }

    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) {
        snprintf(err, sizeof(err), "unable to find streams - %s", av_err2str(ret));
        *err_ = strdup(err);
        goto cleanup;
    }

    audio_stream_idx = av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_stream_idx < 0) {
	ret = FFMPEG_NOT_AUDIO;
        *err_ = strdup("unable to find audio stream");
	goto cleanup;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        *err_ = strdup("unable to alloc pkt");
	ret = ENOMEM;
        goto cleanup;
    }

    ret = av_hash_alloc(&hash, "sha512");
    if (ret < 0) {
        snprintf(err, sizeof(err), "failed to alloc hash - %s\n", ret == EINVAL ? "unknown hash" : strerror(ret));
        *err_ = strdup(err);
	if (ret != EINVAL)  {
	    ret = ENOMEM;
	}
        goto cleanup;
    }
    av_hash_init(hash);

    while ((ret = av_read_frame(ctx, pkt)) >= 0)
    {
	if (pkt->stream_index != audio_stream_idx) {
	    av_packet_unref(pkt);
	    continue;
	}

	av_hash_update(hash, pkt->data, pkt->size);
	av_packet_unref(pkt);
	pktnum++;
    }

    char  res[2 * AV_HASH_MAX_SIZE + 4] = { 0 };
    av_hash_final_hex(hash, res, sizeof(res));

    strncpy(info_->audiohash, res, FFMPEG_AUDIO_HASH_MAXLEN);

#define meta_offsetof(field) offsetof(ffmpeg_meta_t, field)
    struct meta {
        const char*  name;
	size_t  offset;
    } tags[] = {
        { "ARTIST", meta_offsetof(artist) },
	{ "ALBUM",  meta_offsetof(album)  },
	{ "TITLE",  meta_offsetof(title)  },
	{ "GENRE",  meta_offsetof(genre)  },
	{ NULL,     -1 }
    };
    const struct meta*  pt = tags;
    AVDictionaryEntry*  tag = NULL;
    while (pt->name)
    {
	if ((tag = av_dict_get(ctx->metadata, pt->name, NULL, 0)) == NULL) {
	    tag = av_dict_get(ctx->streams[audio_stream_idx]->metadata, pt->name, NULL, 0);
	}

	if (tag) {
	    char**  value = (char**)((char*)(&info_->meta) + pt->offset);
	    *value = strdup(tag->value);
	}
	++pt;
    }
    info_->hash = djbhash(info_->audiohash);

    ret = 0;

cleanup:
    if (pkt)   av_packet_free(&pkt);
    if (ctx) { avformat_close_input(&ctx); avformat_close_input(&ctx); }
    if (hash)  av_hash_freep(&hash);

    return ret;
#endif
}

uint32_t  djbhash(const char* str_)
{
    uint32_t  hash = 5381L;
    int  c;
    while ((c = *str_++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

#ifdef FFMPEG_UNIT_TEST
int main(int argc, char* argv[])
{
    int optind = 1;
    char*  path;
    char*  err = NULL;
    ffmpeg_t*  info = NULL;

    while (optind < argc)
    {
	info = ffmpeg_alloc();
	err = NULL;
	path = argv[optind++];

        int  ret = ffmpeg_audioinfo(info, path, &err);
	printf("%s  %s { arist: '%s' album: '%s' title: '%s' genre: '%s' }  %s\n",
		    info->audiohash, path,
		    info->meta.artist, info->meta.album, info->meta.title, info->meta.genre,
		    ret == FFMPEG_NOT_AUDIO ? "not audio file" : err ? err : "");
	free(err);
	ffmpeg_free(info);
    }

    return 0;
}
#endif
