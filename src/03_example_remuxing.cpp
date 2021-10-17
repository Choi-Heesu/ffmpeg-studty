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

static FileContext input_file_ctx, output_file_ctx;

static int open_input(const char* filename);
static int create_output(const char* filename);
static void release();

int main(int argc, const char** argv) {
  // FFmpeg 라이브러리의 로그 레벨을 지정할 수 있음
  av_log_set_level(AV_LOG_INFO);

  if (argc < 3) {
    printf("Not enough arguments entered.\n");
    return 0;
  }

  if (open_input(argv[1]) < 0) {
    release();
    return 0;
  }

  if (create_output(argv[2]) < 0) {
    release();
    return 0;
  }

  // 파일에 대한 정보를 출력한다
  av_dump_format(output_file_ctx.av_format_ctx, 0, output_file_ctx.av_format_ctx->url, 1);

  AVPacket av_packet;
  int out_stream_index;
  int ret;

  // 입력 스트림에서 패킷을 하나씩 출력 스트림으로 복사한다
  while (true) {
    ret = av_read_frame(input_file_ctx.av_format_ctx, &av_packet);
    if (ret == AVERROR_EOF) {
      printf("End of frame\n");
      break;
    }

    if (av_packet.stream_index != input_file_ctx.v_index &&
        av_packet.stream_index != input_file_ctx.a_index) {
      av_packet_unref(&av_packet);
      continue;
    }

    AVStream* in_stream = input_file_ctx.av_format_ctx->streams[av_packet.stream_index];
    out_stream_index = (av_packet.stream_index == input_file_ctx.v_index ? output_file_ctx.v_index
                                                                         : output_file_ctx.a_index);
    AVStream* out_stream = output_file_ctx.av_format_ctx->streams[out_stream_index];

    // 패킷의 PTS, DTS, Duration을 다시 계산한다
    av_packet.pts = av_rescale_q_rnd(av_packet.pts, in_stream->time_base, out_stream->time_base,
                                     AV_ROUND_NEAR_INF);
    av_packet.dts = av_rescale_q_rnd(av_packet.dts, in_stream->time_base, out_stream->time_base,
                                     AV_ROUND_NEAR_INF);
    av_packet.duration =
        av_rescale_q(av_packet.duration, in_stream->time_base, out_stream->time_base);

    // pos는 스트림의 byte 위치를 의미하며 알 수 없는 경우 -1로 표시한다
    av_packet.pos = -1;
    av_packet.stream_index = out_stream_index;

    // 다시 계산한 패킷 정보를 AVFormatContext에 입력한다
    if (av_interleaved_write_frame(output_file_ctx.av_format_ctx, &av_packet) < 0) {
      printf("Error occured when writing packet into file\n");
      break;
    }
    av_packet_unref(&av_packet);
  }

  // AVPacket을 쓰는 시점에 정리하지 못한 정보들을 출력 미디어 파일에 쓴다
  // (moov 헤더처럼 모든 스트림 정보가 있어야 추가할 수 있는 정보들이 있음)
  av_write_trailer(output_file_ctx.av_format_ctx);
  release();

  return 0;
}

static int open_input(const char* filename) {
  input_file_ctx.av_format_ctx = NULL;
  input_file_ctx.v_index = input_file_ctx.a_index = -1;

  if (avformat_open_input(&input_file_ctx.av_format_ctx, filename, NULL, NULL) < 0) {
    printf("Couln't open video file\n");
    return -1;
  }

  if (avformat_find_stream_info(input_file_ctx.av_format_ctx, NULL) < 0) {
    printf("Failed to retrieve input stream information\n");
    return -1;
  }

  for (int index = 0; index < input_file_ctx.av_format_ctx->nb_streams; ++index) {
    AVCodecParameters* av_codec_params = input_file_ctx.av_format_ctx->streams[index]->codecpar;
    if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO && input_file_ctx.v_index < 0) {
      input_file_ctx.v_index = index;
    } else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO && input_file_ctx.a_index < 0) {
      input_file_ctx.a_index = index;
    }
  }

  if (input_file_ctx.v_index < 0 && input_file_ctx.a_index < 0) {
    printf("Failed to retrieve input stream information\n");
    return -1;
  }

  return 0;
}

static int create_output(const char* filename) {
  output_file_ctx.av_format_ctx = NULL;
  output_file_ctx.v_index = output_file_ctx.a_index = -1;

  if (avformat_alloc_output_context2(&output_file_ctx.av_format_ctx, NULL, NULL, filename) < 0) {
    printf("Couldn't create output file context\n");
    return -1;
  }

  int out_index = 0;
  for (int index = 0; index < input_file_ctx.av_format_ctx->nb_streams; ++index) {
    if (index != input_file_ctx.v_index && index != input_file_ctx.a_index) {
      continue;
    }

    AVStream* in_stream = input_file_ctx.av_format_ctx->streams[index];
    AVCodecParameters* in_codec_params = in_stream->codecpar;

    // 새로운 스트림을 생성한다
    AVStream* out_stream = avformat_new_stream(output_file_ctx.av_format_ctx, NULL);

    if (!out_stream) {
      printf("Failed to allocate output stream\n");
      return -1;
    }

    // 새로운 스트림에 AVCodecParameters 복사
    if (avcodec_parameters_copy(out_stream->codecpar, in_codec_params) < 0) {
      printf("Error occurred while copying AVCodecParameters\n");
      return -1;
    }

    if (index == input_file_ctx.v_index) {
      output_file_ctx.v_index = out_index++;
    } else {
      output_file_ctx.a_index = out_index++;
    }
  }

  // avio_open()은 fopen과 같은 역할을 하는 함수로 아무것도 쓰이지 않은 빈 파일을 생성할 때 사용한다
  if (!(output_file_ctx.av_format_ctx->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&output_file_ctx.av_format_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
      printf("Failed to create output file\n");
      return -1;
    }
  }

  // avformat_write_header()는 컨테이너의 규격에 맞는 헤더를 생성하는 함수로 AVFormatContext의
  // 컨테이너 정보와 AVStream의 스트림 정보를 기반으로 헤더를 쓴다
  if (avformat_write_header(output_file_ctx.av_format_ctx, NULL) < 0) {
    printf("Failed writing header into output file\n");
    return -1;
  }

  return 0;
}

static void release() {
  // avformat_open_input()으로 메모리를 할당했으면 avformat_close_input()으로 해제해야 메모리 릭이
  // 발생하지 않는다
  if (input_file_ctx.av_format_ctx) {
    avformat_close_input(&input_file_ctx.av_format_ctx);
  }

  if (!(output_file_ctx.av_format_ctx->oformat->flags & AVFMT_NOFILE)) {
    avio_closep(&output_file_ctx.av_format_ctx->pb);
  }
  // AVFormatContext 내부에서 할당한 메모리를 해제한다
  avformat_free_context(output_file_ctx.av_format_ctx);
}