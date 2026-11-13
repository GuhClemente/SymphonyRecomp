// SPDX-License-Identifier: AGPL-3.0-or-later
// An audio file (a PSP .at3, or a WAV) -> s16 WAV through an FFmpeg filter
// chain, single-threaded: FFmpeg's libraries, not the ffmpeg program (since
// 7.0 it needs threads, which a page opened from file:// cannot have). The
// chain sets the rate, the channels and s16, e.g.
//   highpass=f=50,volume=0.5,aresample=37800,aformat=sample_fmts=s16:channel_layouts=mono
// The SOTN Ultimate builder's audio decoder
// (mods/sotn-ultimate/tools/builder/native/build.sh links it with FFmpeg 7.1.1
// and mods/sotn-ultimate/tools/satxa.c into one WebAssembly module; the same
// module runs in the release build, through Node, and in the player's builder
// page, so both make the same bytes).
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void le32(uint8_t* p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

static int write_wav(
    const char* path, const int16_t* s, size_t n, int rate, int ch) {
    uint8_t h[44] = "RIFF....WAVEfmt ";
    FILE* f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    le32(h + 4, 36 + n * 2);
    le32(h + 16, 16);
    h[20] = 1; // PCM
    h[22] = ch;
    le32(h + 24, rate);
    le32(h + 28, rate * 2 * ch);
    h[32] = 2 * ch;
    h[34] = 16;
    memcpy(h + 36, "data", 4);
    le32(h + 40, n * 2);
    fwrite(h, 1, 44, f);
    fwrite(s, 2, n, f);
    fclose(f);
    return 0;
}

// returns the sample frames written, < 0 on error
int at3_decode(const char* in, const char* out, const char* chain) {
    AVFormatContext* fmt = NULL;
    AVCodecContext* dec = NULL;
    AVFilterGraph* graph = NULL;
    AVFilterContext *src = NULL, *sink = NULL;
    AVFilterInOut *outputs = NULL, *inputs = NULL;
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* filt = av_frame_alloc();
    AVChannelLayout layout_out = {0};
    int16_t* pcm = NULL;
    size_t n = 0, cap = 0;
    int ret, stream;
    char args[256], layout[64];

    if ((ret = avformat_open_input(&fmt, in, NULL, NULL)) < 0 ||
        (ret = avformat_find_stream_info(fmt, NULL)) < 0) {
        goto end;
    }
    stream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (stream < 0) {
        ret = stream;
        goto end;
    }
    const AVCodec* codec =
        avcodec_find_decoder(fmt->streams[stream]->codecpar->codec_id);
    dec = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec, fmt->streams[stream]->codecpar);
    dec->thread_count = 1;
    if ((ret = avcodec_open2(dec, codec, NULL)) < 0) {
        goto end;
    }
    // a plain WAV (the Saturn takes, psp_dub.py) has no channel mask: an
    // "unspecified" layout, whose channels the biquad filters (highpass)
    // skip, copying them through unfiltered. The ffmpeg program assigns
    // the default layout here, so does this
    if (dec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) {
        int n = dec->ch_layout.nb_channels;
        av_channel_layout_uninit(&dec->ch_layout);
        av_channel_layout_default(&dec->ch_layout, n);
    }

    graph = avfilter_graph_alloc();
    graph->nb_threads = 1;
    av_channel_layout_describe(&dec->ch_layout, layout, sizeof(layout));
    snprintf(args, sizeof(args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
             dec->sample_rate, dec->sample_rate,
             av_get_sample_fmt_name(dec->sample_fmt), layout);
    if ((ret = avfilter_graph_create_filter(
             &src, avfilter_get_by_name("abuffer"), "in", args, NULL, graph)) <
            0 ||
        (ret = avfilter_graph_create_filter(
             &sink, avfilter_get_by_name("abuffersink"), "out", NULL, NULL,
             graph)) < 0) {
        goto end;
    }
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    outputs->name = av_strdup("in");
    outputs->filter_ctx = src;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink;
    if ((ret = avfilter_graph_parse_ptr(
             graph, chain, &inputs, &outputs, NULL)) < 0 ||
        (ret = avfilter_graph_config(graph, NULL)) < 0) {
        goto end;
    }
    if (av_buffersink_get_format(sink) != AV_SAMPLE_FMT_S16 ||
        av_buffersink_get_ch_layout(sink, &layout_out) < 0) {
        ret = AVERROR(EINVAL); // the chain must end in packed s16
        goto end;
    }

    for (int flushing = 0;;) {
        if (!flushing) {
            ret = av_read_frame(fmt, pkt);
            if (ret < 0) {
                flushing = 1;
                avcodec_send_packet(dec, NULL);
            } else if (pkt->stream_index == stream) {
                avcodec_send_packet(dec, pkt);
            }
            av_packet_unref(pkt);
        }
        while ((ret = avcodec_receive_frame(dec, frame)) >= 0) {
            if ((ret = av_buffersrc_add_frame(src, frame)) < 0) {
                goto end;
            }
        }
        if (flushing && ret == AVERROR_EOF) {
            if ((ret = av_buffersrc_add_frame(src, NULL)) < 0) {
                goto end;
            }
        }
        while ((ret = av_buffersink_get_frame(sink, filt)) >= 0) {
            // packed s16: one plane, the channels interleaved
            size_t k = (size_t)filt->nb_samples * layout_out.nb_channels;
            if (n + k > cap) {
                cap = (n + k) * 2;
                pcm = realloc(pcm, cap * 2);
            }
            memcpy(pcm + n, filt->data[0], k * 2);
            n += k;
            av_frame_unref(filt);
        }
        if (ret == AVERROR_EOF) {
            break;
        }
    }
    ret = write_wav(out, pcm, n, av_buffersink_get_sample_rate(sink),
                    layout_out.nb_channels) < 0
              ? -1
              : (int)(n / layout_out.nb_channels);

end:
    free(pcm);
    av_channel_layout_uninit(&layout_out);
    av_frame_free(&frame);
    av_frame_free(&filt);
    av_packet_free(&pkt);
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avfilter_graph_free(&graph);
    avcodec_free_context(&dec);
    avformat_close_input(&fmt);
    return ret;
}
