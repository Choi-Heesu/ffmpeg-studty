extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
}

struct FileContext {
  AVFormatContext* av_format_ctx;
  int v_index;
  int a_index;
};

static FileContext input_ctx;

static int open_input(const char* filename);
static void release();

int main(int argc, const char** argv) {
  // FFmpeg 라이브러리의 로그 레벨을 지정할 수 있음
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Couldn't find video file\n");
    return 0;
  }

  if (open_input(argv[1]) < 0) {
    release();
    return 0;
  }

  // AVPacket은 코덱으로 압축된 스트림 데이터를 저장하는 데 사용
  AVPacket av_packet;
  int ret;

  while (true) {
    // AVFormatContext로부터 패킷을 순서대로 읽어 AVPacket에 저장
    ret = av_read_frame(input_ctx.av_format_ctx, &av_packet);
    if (ret == AVERROR_EOF) {
      //더 이상 읽어올 패킷이 없음
      printf("End of frame");
      break;
    }

    if (av_packet.stream_index == input_ctx.v_index) {
      printf("Video packet\n");
    } else if (av_packet.stream_index == input_ctx.a_index) {
      printf("Audio packet\n");
    }

    // AVPacket 내부에서 할당한 메모리 해제
    av_packet_unref(&av_packet);
  }

  release();

  return 0;
}

static int open_input(const char* filename) {
  input_ctx.av_format_ctx = NULL;
  input_ctx.v_index = input_ctx.a_index = -1;

  if (avformat_open_input(&input_ctx.av_format_ctx, filename, NULL, NULL) < 0) {
    printf("Couln't open video file\n");
    return -1;
  }

  if (avformat_find_stream_info(input_ctx.av_format_ctx, NULL) < 0) {
    printf("Failed to retrieve input stream information\n");
    return -1;
  }

  for (int index = 0; index < input_ctx.av_format_ctx->nb_streams; ++index) {
    AVCodecParameters* av_codec_params = input_ctx.av_format_ctx->streams[index]->codecpar;
    if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO && input_ctx.v_index < 0) {
      input_ctx.v_index = index;
    } else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO && input_ctx.a_index < 0) {
      input_ctx.a_index = index;
    }
  }

  if (input_ctx.v_index < 0 && input_ctx.a_index < 0) {
    printf("Failed to retrieve input stream information\n");
    return -1;
  }

  return 0;
}

static void release() {
  if (input_ctx.av_format_ctx) {
    avformat_close_input(&input_ctx.av_format_ctx);
  }
}