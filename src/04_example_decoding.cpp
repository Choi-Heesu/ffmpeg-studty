extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <stdio.h>
}

struct FileContext {
  AVFormatContext* av_format_ctx;
  AVCodecContext* video_codec_ctx;
  AVCodecContext* audio_codec_ctx;
  int v_index;
  int a_index;
};

FileContext input_file_ctx;

int open_input(const char* filename);
int open_decoder(AVCodecParameters* av_codec_params, AVCodecContext** av_codec_ctx);
int decode_packet(AVCodecContext** av_codec_ctx, AVPacket* av_packet, AVFrame** av_frame);
void release();

int main(int argc, const char** argv) {
  // FFmpeg 라이브러리의 로그 레벨을 지정할 수 있음
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Not enough arugments entered\n");
    return -1;
  }

  if (open_input(argv[1]) < 0) {
    release();
    return -1;
  }

  // AVFrame은 디코딩한 raw 데이터를 저장하는 구조체
  AVFrame* decoded_frame = av_frame_alloc();
  if (!decoded_frame) {
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
      printf("-----------------------\n");
      if ((*av_codec_ctx)->codec_type == AVMEDIA_TYPE_VIDEO) {
        printf("Video : frame->width, height : %dx%d\n", decoded_frame->width,
               decoded_frame->height);
        printf("Video : frame->sample_aspect_ratio : %d/%d\n",
               decoded_frame->sample_aspect_ratio.num, decoded_frame->sample_aspect_ratio.den);
      } else {
        printf("Audio : frame->nb_samples : %d\n", decoded_frame->nb_samples);
        printf("Audio : frame->channels : %d\n", decoded_frame->channels);
      }
      av_frame_unref(decoded_frame);
    }
    av_packet_unref(&av_packet);
  }

  release();

  return 0;
}

int open_input(const char* filename) {
  input_file_ctx.av_format_ctx = NULL;
  input_file_ctx.video_codec_ctx = NULL;
  input_file_ctx.audio_codec_ctx = NULL;
  input_file_ctx.v_index = input_file_ctx.a_index = -1;

  if (avformat_open_input(&(input_file_ctx.av_format_ctx), filename, NULL, NULL) < 0) {
    printf("Couldn't open input file %s\n", filename);
    return -1;
  }

  if (avformat_find_stream_info(input_file_ctx.av_format_ctx, NULL) < 0) {
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
  // 코덱 ID를 통해 FFmpeg 라이브러리가 자동으로 코덱을 찾도록 함
  AVCodec* av_decoder = avcodec_find_decoder(av_codec_params->codec_id);
  if (!av_decoder) {
    printf("Couln't find AVCodec\n");
    return -1;
  }

  *av_codec_ctx = avcodec_alloc_context3(av_decoder);
  if (!*av_codec_ctx) {
    printf("Couldn't create AVCodecContext\n");
    return -1;
  }

  if (avcodec_parameters_to_context(*av_codec_ctx, av_codec_params) < 0) {
    printf("Couldn't initalize AVCodecContext\n");
    return -1;
  }

  if (avcodec_open2(*av_codec_ctx, av_decoder, NULL) < 0) {
    printf("Couln't open codec\n");
    return -1;
  }

  return 0;
}

int decode_packet(AVCodecContext** av_codec_ctx, AVPacket* av_packet, AVFrame** av_frame) {
  int ret = 0;

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
}