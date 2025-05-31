#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmdline_parser.h"
#include "tld_tracker.h"
#include "unit_tests.h"
#include "profile.h"
#include <stdint.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

// Application Modes
typedef enum {
    APP_MODE_NONE,
    APP_MODE_TEST,
    APP_MODE_WEBCAM,
    APP_MODE_VIDEOFILE
} AppModes;

/*void print_help() {
    printf("=== TLD tracker demo application ===\n");
    printf("Command line options template: '--key=val' or '--key'\n");
    printf("\t--help\t outputs help reference\n");
    printf("\t--mode=MODE\t runs application in appropriate mode. MODE may be set as 'test', 'webcam' or 'video'\n");
    printf("\t--webcam\t same as '--mode=webcam'\n");
    printf("\t--test\t same as '--mode=test'\n");
    printf("\t--video\t same as '--mode=video'\n");
    printf("\t--camid=ID\t selects active camera device with appropriate ID in 'webcam' mode\n");
    printf("\t--videopath=PATH\t selects active video file with appropriate absolute path\n");
    printf("\t--debug\t enables debug window with intermediate processing results\n");
    printf("\n");
}*/

void avframe_to_gray_image(const AVFrame *frame, Image *out) {
    int width = frame->width;
    int height = frame->height;
    out->width = width;
    out->height = height;
    out->data = malloc(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t r = frame->data[0][y * frame->linesize[0] + 3*x + 0];
            uint8_t g = frame->data[0][y * frame->linesize[0] + 3*x + 1];
            uint8_t b = frame->data[0][y * frame->linesize[0] + 3*x + 2];
            uint8_t gray = (uint8_t)((r * 30 + g * 59 + b * 11) / 100); // ITU-R BT.601
            out->data[y * width + x] = gray;
        }
    }
}

// Reads the next frame from video, converts to RGB24, returns 1 if frame is read, 0 if end/error
int get_next_gray_frame(AVFormatContext *fmt_ctx, AVCodecContext *dec_ctx, int video_stream_idx,
                        struct SwsContext *sws_ctx, AVFrame *frame_rgb, Image *out_img) {
    AVPacket pkt;
    int got_frame = 0;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_idx) {
            AVFrame *frame = av_frame_alloc();
            int ret, got_picture;
            ret = avcodec_send_packet(dec_ctx, &pkt);
            if (ret < 0) { av_frame_free(&frame); av_packet_unref(&pkt); continue; }
            got_picture = avcodec_receive_frame(dec_ctx, frame);
            if (got_picture == 0) {
                // Convert to RGB24
                sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0, dec_ctx->height,
                          frame_rgb->data, frame_rgb->linesize);
                // Convert RGB24 to grayscale Image
                avframe_to_gray_image(frame_rgb, out_img);
                got_frame = 1;
                av_frame_free(&frame);
                av_packet_unref(&pkt);
                break;
            }
            av_frame_free(&frame);
        }
        av_packet_unref(&pkt);
    }
    return got_frame;
}

// Main tracking loop
void run_app_ffmpeg(const char *video_path, int debug) {
    av_register_all();
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, video_path, NULL, NULL) != 0) {
        fprintf(stderr, "Failed to open video file: %s\n", video_path);
        return;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Failed to read stream info\n");
        avformat_close_input(&fmt_ctx);
        return;
    }
    int video_stream_idx = -1;
    AVCodec *codec = NULL;
    AVCodecParameters *codecpar = NULL;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            codecpar = fmt_ctx->streams[i]->codecpar;
            break;
        }
    }
    if (video_stream_idx == -1) {
        fprintf(stderr, "No video stream found\n");
        avformat_close_input(&fmt_ctx);
        return;
    }
    codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec_ctx, codecpar);
    avcodec_open2(dec_ctx, codec, NULL);

    int width = dec_ctx->width;
    int height = dec_ctx->height;
    struct SwsContext *sws_ctx = sws_getContext(width, height, dec_ctx->pix_fmt,
                                                width, height, AV_PIX_FMT_RGB24,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    AVFrame *frame_rgb = av_frame_alloc();
    int num_bytes = avpicture_get_size(AV_PIX_FMT_RGB24, width, height);
    uint8_t *buffer = av_malloc(num_bytes);
    avpicture_fill((AVPicture*)frame_rgb, buffer, AV_PIX_FMT_RGB24, width, height);

    TldTracker tracker;
    tld_tracker_init(&tracker, default_settings());
    size_t frame_id = 0;
    while (1) {
        Image gray;
        int got = get_next_gray_frame(fmt_ctx, dec_ctx, video_stream_idx, sws_ctx, frame_rgb, &gray);
        if (!got) break;

        Candidate result = tld_tracker_process_frame(&tracker, &gray);

        if (debug) {
            printf("[DEBUG] Frame %zu: Candidate @ (%d, %d, %d, %d), prob=%.3f\n",
                frame_id, result.strobe.x, result.strobe.y, result.strobe.width, result.strobe.height, result.prob);
        }
        draw_candidate(&gray, result);

        // Optionally save debug images (implement save_image_png or similar)
        // char fname[128];
        // sprintf(fname, "frame_%zu.png", frame_id);
        // save_image_png(fname, &gray);

        if (frame_id == 0) {
            Rect bbox = {117,231,105,79};
            tld_tracker_start_tracking(&tracker, bbox);
        }
        free(gray.data);
        frame_id++;
    }

    tld_tracker_free(&tracker);
    av_free(buffer);
    av_frame_free(&frame_rgb);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
}
int main(int argc, char** argv) {
    const char *video_path = "/mnt/tmp/scene1.mp4";
    int debug = 0;

    // Minimal argument parsing
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--videopath=PATH] [--debug]\n", argv[0]);
            printf("  --videopath=PATH  Set input video file path (default: %s)\n", video_path);
            printf("  --debug           Enable debug print\n");
            return 0;
        }
        if (strncmp(argv[i], "--videopath=", 12) == 0) {
            video_path = argv[i] + 12;
        }
        if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        }
    }

    // Call the tracking runner
    run_app_ffmpeg(video_path, debug);

    return 0;
}

