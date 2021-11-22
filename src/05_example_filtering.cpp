extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
}
#include <cstdio>

struct FileContext {
  AVFormatContext* av_format_ctx;
  AVCodecContext* video_codec_ctx;
  AVCodecContext* audio_codec_ctx;
  int v_index;
  int a_index;
};

struct FilterContext {
  AVFilterGraph* av_filter_graph;
  AVFilterContext* src_filter_ctx;
  AVFilterContext* sink_filter_ctx;
};

FileContext input_file_ctx;
FilterContext video_filter_ctx, audio_filter_ctx;

const int dst_width = 480;
const int dst_height = 320;
const int64_t dst_ch_layout = AV_CH_LAYOUT_MONO;
const int dst_sample_rate = 32000;

int open_input(const char* filename);
int open_decoder(AVCodecParameters* av_codec_params, AVCodecContext** av_codec_ctx);
int decode_packet(AVCodecContext** av_codec_ctx, AVPacket* av_packet, AVFrame** av_frame);
int init_video_filter();
int init_audio_filter();
void release();

int main(int argc, const char** argv) {
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Not enough arguments entered\n");
    return -1;
  }

  if (open_input(argv[1]) < 0) {
    release();
    return -1;
  }

  if (init_video_filter() < 0) {
    release();
    return -1;
  }

  if (init_audio_filter() < 0) {
    release();
    return -1;
  }

  AVFrame* decoded_frame = av_frame_alloc();
  if (!decoded_frame) {
    release();
    return -1;
  }

  AVFrame* filtered_frame = av_frame_alloc();
  if (!filtered_frame) {
    av_frame_free(&decoded_frame);
    release();
    return -1;
  }

  AVPacket av_packet;
  while (true) {
    int ret = av_read_frame(input_file_ctx.av_format_ctx, &av_packet);
    if (ret == AVERROR_EOF) {
      printf("End of frame\n");
      break;
    }
    if (av_packet.stream_index != input_file_ctx.v_index &&
        av_packet.stream_index != input_file_ctx.a_index) {
      av_packet_unref(&av_packet);
      continue;
    }

    AVStream* av_stream = input_file_ctx.av_format_ctx->streams[av_packet.stream_index];
    AVCodecContext** av_codec_ctx;
    if (av_packet.stream_index == input_file_ctx.v_index) {
      av_codec_ctx = &(input_file_ctx.video_codec_ctx);
    } else {
      av_codec_ctx = &(input_file_ctx.audio_codec_ctx);
    }

    av_packet_rescale_ts(&av_packet, av_stream->time_base, (*av_codec_ctx)->time_base);

    ret = decode_packet(av_codec_ctx, &av_packet, &decoded_frame);
    if (ret >= 0) {
      FilterContext* av_filter_ctx;
      if (av_packet.stream_index == input_file_ctx.v_index) {
        av_filter_ctx = &video_filter_ctx;
        printf("[before] Video : resolution : %dx%d\n", decoded_frame->width,
               decoded_frame->height);
      } else {
        av_filter_ctx = &audio_filter_ctx;
        printf("[before] Audio : sample_rate : %d / channels : %d\n", decoded_frame->sample_rate,
               decoded_frame->channels);
      }

      if (av_buffersrc_add_frame(av_filter_ctx->src_filter_ctx, decoded_frame) < 0) {
        printf("Error occurred when putting frame into filter context\n");
        break;
      }

      while (true) {
        if (av_buffersink_get_frame(av_filter_ctx->sink_filter_ctx, filtered_frame) < 0) {
          break;
        }

        if (av_packet.stream_index == input_file_ctx.v_index) {
          printf("[after] Video : resolution : %dx%d\n", filtered_frame->width,
                 filtered_frame->height);
        } else {
          printf("[after] Audio : sample_rate : %d / channels : %d\n", filtered_frame->sample_rate,
                 filtered_frame->channels);
        }

        av_frame_unref(filtered_frame);
      }
      av_frame_unref(decoded_frame);
    }
    av_packet_unref(&av_packet);
  }

  av_frame_free(&decoded_frame);
  av_frame_free(&filtered_frame);

  release();

  return 0;
}

int open_input(const char* filename) {
  input_file_ctx.av_format_ctx = nullptr;
  input_file_ctx.video_codec_ctx = nullptr;
  input_file_ctx.audio_codec_ctx = nullptr;
  input_file_ctx.v_index = input_file_ctx.a_index = -1;

  if (avformat_open_input(&(input_file_ctx.av_format_ctx), filename, nullptr, nullptr) < 0) {
    printf("Couldn't open input file %s\n", filename);
    return -1;
  }

  if (avformat_find_stream_info(input_file_ctx.av_format_ctx, nullptr) < 0) {
    printf("Failed to retrieve input stream information\n");
    return -1;
  }

  for (int index = 0; index < input_file_ctx.av_format_ctx->nb_streams; ++index) {
    AVCodecParameters* av_codec_params = input_file_ctx.av_format_ctx->streams[index]->codecpar;
    if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO && input_file_ctx.v_index < 0) {
      if (open_decoder(av_codec_params, &(input_file_ctx.video_codec_ctx)) < 0) {
        break;
      }
      input_file_ctx.v_index = index;
    } else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO && input_file_ctx.a_index < 0) {
      if (open_decoder(av_codec_params, &(input_file_ctx.audio_codec_ctx)) < 0) {
        break;
      }
      input_file_ctx.a_index = index;
    }

    if (input_file_ctx.v_index < 0 && input_file_ctx.a_index < 0) {
      printf("Failed to retrieve input stream information\n");
    }
  }

  return 0;
}

int open_decoder(AVCodecParameters* av_codec_params, AVCodecContext** av_codec_ctx) {
  AVCodec* av_decoder = avcodec_find_decoder(av_codec_params->codec_id);
  if (!av_decoder) {
    printf("Couldn't find AVCodec\n");
    return -1;
  }

  *av_codec_ctx = avcodec_alloc_context3(av_decoder);
  if (!*av_codec_ctx) {
    printf("Couldn't create AVCodecContext\n");
    return -1;
  }

  if (avcodec_parameters_to_context(*av_codec_ctx, av_codec_params) < 0) {
    printf("Couldn't initialize AVCodecContext\n");
    return -1;
  }

  if (avcodec_open2(*av_codec_ctx, av_decoder, nullptr) < 0) {
    printf("Couldn't open codec\n");
    return -1;
  }

  return 0;
}

int decode_packet(AVCodecContext** av_codec_ctx, AVPacket* av_packet, AVFrame** av_frame) {
  int ret;

  ret = avcodec_send_packet(*av_codec_ctx, av_packet);
  if (ret < 0) {
    printf("Couldn't send AVPacket");
    return ret;
  }

  ret = avcodec_receive_frame(*av_codec_ctx, *av_frame);
  if (ret >= 0) {
    (*av_frame)->pts = (*av_frame)->best_effort_timestamp;
  }

  return ret;
}

int init_video_filter() {
  AVStream* av_stream = input_file_ctx.av_format_ctx->streams[input_file_ctx.v_index];
  AVCodecContext* av_codec_ctx = input_file_ctx.video_codec_ctx;

  AVFilterContext* rescale_filter;
  AVFilterInOut* inputs;
  AVFilterInOut* outputs;
  char args[512];

  video_filter_ctx.av_filter_graph = nullptr;
  video_filter_ctx.src_filter_ctx = nullptr;
  video_filter_ctx.sink_filter_ctx = nullptr;

  video_filter_ctx.av_filter_graph = avfilter_graph_alloc();
  if (!video_filter_ctx.av_filter_graph) {
    return -1;
  }

  if (avfilter_graph_parse2(video_filter_ctx.av_filter_graph, "null", &inputs, &outputs) < 0) {
    printf("Failed to parse video filter graph\n");
    return -1;
  }

  snprintf(args, sizeof(args), "time_base=%d/%d:video_size=%dx%d:pix_fmt=%d:pixel_aspect=%d/%d",
           av_stream->time_base.num, av_stream->time_base.den, av_codec_ctx->width,
           av_codec_ctx->height, av_codec_ctx->pix_fmt, av_codec_ctx->sample_aspect_ratio.num,
           av_codec_ctx->sample_aspect_ratio.den);
  if (avfilter_graph_create_filter(&video_filter_ctx.src_filter_ctx, avfilter_get_by_name("buffer"),
                                   "in", args, nullptr, video_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create video buffer source\n");
    return -1;
  }

  if (avfilter_link(video_filter_ctx.src_filter_ctx, 0, inputs->filter_ctx, 0) < 0) {
    printf("Failed to link video buffer source\n");
    return -1;
  }

  if (avfilter_graph_create_filter(&video_filter_ctx.sink_filter_ctx,
                                   avfilter_get_by_name("buffersink"), "out", nullptr, nullptr,
                                   video_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create video buffer sink\n");
    return -1;
  }

  snprintf(args, sizeof(args), "%d:%d", dst_width, dst_height);

  if (avfilter_graph_create_filter(&rescale_filter, avfilter_get_by_name("scale"), "scale", args,
                                   nullptr, video_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create video scale filter\n");
    return -1;
  }

  if (avfilter_link(outputs->filter_ctx, 0, rescale_filter, 0) < 0) {
    printf("Failed to link video format filter\n");
    return -1;
  }

  if (avfilter_link(rescale_filter, 0, video_filter_ctx.sink_filter_ctx, 0) < 0) {
    printf("Failed to link video format filter\n");
    return -1;
  }

  if (avfilter_graph_config(video_filter_ctx.av_filter_graph, nullptr) < 0) {
    printf("Failed to configure video filter context\n");
    return -1;
  }

  av_buffersink_set_frame_size(video_filter_ctx.sink_filter_ctx, av_codec_ctx->frame_size);

  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return 1;
}

int init_audio_filter() {
  AVStream* av_stream = input_file_ctx.av_format_ctx->streams[input_file_ctx.a_index];
  AVCodecContext* av_codec_ctx = input_file_ctx.audio_codec_ctx;

  AVFilterContext* resample_filter;
  AVFilterInOut* inputs;
  AVFilterInOut* outputs;
  char args[512];

  audio_filter_ctx.av_filter_graph = nullptr;
  audio_filter_ctx.src_filter_ctx = nullptr;
  audio_filter_ctx.sink_filter_ctx = nullptr;

  audio_filter_ctx.av_filter_graph = avfilter_graph_alloc();
  if (!audio_filter_ctx.av_filter_graph) {
    return -1;
  }

  if (avfilter_graph_parse2(audio_filter_ctx.av_filter_graph, "anull", &inputs, &outputs) < 0) {
    printf("Failed to parse audio filter graph\n");
    return -1;
  }

  snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llu",
           av_stream->time_base.num, av_stream->time_base.den, av_codec_ctx->sample_rate,
           av_get_sample_fmt_name(av_codec_ctx->sample_fmt), av_codec_ctx->channel_layout);

  if (avfilter_graph_create_filter(&audio_filter_ctx.src_filter_ctx,
                                   avfilter_get_by_name("abuffer"), "in", args, nullptr,
                                   audio_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create audio buffer source\n");
    return -1;
  }

  if (avfilter_link(audio_filter_ctx.src_filter_ctx, 0, inputs->filter_ctx, 0) < 0) {
    printf("Failed to link audio buffer source\n");
    return -1;
  }

  if (avfilter_graph_create_filter(&audio_filter_ctx.sink_filter_ctx,
                                   avfilter_get_by_name("abuffersink"), "out", nullptr, nullptr,
                                   audio_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create audio buffer sink\n");
    return -1;
  }

  snprintf(args, sizeof(args), "sample_rates=%d:channel_layouts=0x%llu", dst_sample_rate,
           dst_ch_layout);

  if (avfilter_graph_create_filter(&resample_filter, avfilter_get_by_name("aformat"), "aformat",
                                   args, nullptr, audio_filter_ctx.av_filter_graph) < 0) {
    printf("Failed to create audio format filter\n");
    return -1;
  }

  if (avfilter_link(outputs->filter_ctx, 0, resample_filter, 0) < 0) {
    printf("Failed to link audio format filter\n");
    return -1;
  }

  if (avfilter_link(resample_filter, 0, audio_filter_ctx.sink_filter_ctx, 0) < 0) {
    printf("Failed to link audio format filter\n");
    return -1;
  }

  if (avfilter_graph_config(audio_filter_ctx.av_filter_graph, nullptr) < 0) {
    printf("Failed to configure audio filter context\n");
    return -1;
  }

  av_buffersink_set_frame_size(audio_filter_ctx.sink_filter_ctx, av_codec_ctx->frame_size);

  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return 1;
}

void release() {
  if (input_file_ctx.av_format_ctx) {
    avformat_close_input(&(input_file_ctx.av_format_ctx));
  }

  if (input_file_ctx.video_codec_ctx) {
    avcodec_close(input_file_ctx.video_codec_ctx);
  }

  if (input_file_ctx.audio_codec_ctx) {
    avcodec_close(input_file_ctx.audio_codec_ctx);
  }

  if (video_filter_ctx.av_filter_graph) {
    avfilter_graph_free(&(video_filter_ctx.av_filter_graph));
  }

  if (audio_filter_ctx.av_filter_graph) {
    avfilter_graph_free(&(audio_filter_ctx.av_filter_graph));
  }
}