extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <cstdio>

struct FileContext {
  AVFormatContext* av_format_ctx;
  int v_index;
  int a_index;
};

FileContext input_ctx;

int open_input(const char* filename);
void release();

int main(int argc, const char** argv) {
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Couldn't find video file\n");
    return 0;
  }

  if (open_input(argv[1]) < 0) {
    release();
    return 0;
  }

  // AVPacket 구조체는 코덱으로 압축된 스트림 데이터를 저장하는 데 사용
  AVPacket av_packet;
  int ret;

  while (true) {
    // AVFormatContext 구조체로부터 패킷을 순서대로 읽어 AVPacket 구조체에 저장
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

    // AVPacket 구조체에 할당한 메모리 해제
    av_packet_unref(&av_packet);
  }

  release();

  return 0;
}

int open_input(const char* filename) {
  input_ctx.av_format_ctx = nullptr;
  input_ctx.v_index = input_ctx.a_index = -1;

  if (avformat_open_input(&input_ctx.av_format_ctx, filename, nullptr, nullptr) < 0) {
    printf("Couldn't open video file\n");
    return -1;
  }

  if (avformat_find_stream_info(input_ctx.av_format_ctx, nullptr) < 0) {
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

void release() {
  if (input_ctx.av_format_ctx) {
    avformat_close_input(&input_ctx.av_format_ctx);
  }
}