extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
}

int main(int argc, const char** argv) {
  // FFmpeg 라이브러리의 로그 레벨을 지정할 수 있음
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Couldn't find video file\n");
    return 0;
  }

  // AVFormatContext를 따로 할당하지 않고 NULL로 냅둬도 avforamt_open_input()
  // 메서드가 알아서 할당해서 넘겨줌
  AVFormatContext* av_format_ctx = avformat_alloc_context();

  // AVFormatContext에 파일로부터 읽은 컨테이너 정보를 저장
  if (avformat_open_input(&av_format_ctx, argv[1], NULL, NULL) < 0) {
    printf("Couln't open video file\n");
    return 0;
  }

  // AVFormatContext에 스트림 관련 정보를 찾아서 저장
  if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
    printf("Failed to retrieve input stream information\n");
    return 0;
  }

  for (int i = 0; i < av_format_ctx->nb_streams; ++i) {
    // AVCodecParameters는 스트림에 사용된 코덱 속성에 대한 정보를 가지고 있음
    AVCodecParameters* av_codec_params = av_format_ctx->streams[i]->codecpar;

    if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
      printf("-------- video info --------\n");
      printf("codec_id : %d\n", av_codec_params->codec_id);
      printf("bitrate : %lld\n", av_codec_params->bit_rate);
      printf("width : %d, height : %d\n", av_codec_params->width,
             av_codec_params->height);
    } else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
      printf("-------- audio info --------\n");
      printf("codec_id : %d\n", av_codec_params->codec_id);
      printf("bitrate : %lld\n", av_codec_params->bit_rate);
      printf("sample_rate : %d\n", av_codec_params->sample_rate);
      printf("number of channels : %d\n", av_codec_params->channels);
    }
  }

  // 내부에서 할당한 메모리 해제
  avformat_close_input(&av_format_ctx);

  return 0;
}