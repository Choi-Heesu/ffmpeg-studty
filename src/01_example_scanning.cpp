extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <cstdio>

int main(int argc, const char** argv) {
  // FFmpeg 라이브러리의 로그 레벨을 지정할 수 있음
  av_log_set_level(AV_LOG_INFO);

  if (argc < 2) {
    printf("Couldn't find video file\n");
    return 0;
  }

  // 컨테이너 정보를 담고 있는 AVFormatContext 구조체에 메모리 할당
  AVFormatContext* av_format_ctx = avformat_alloc_context();

  // 만약 AVFormatContext 구조체를 메모리 할당하지 않았으면 avformat_open_input() 메서드가 자동으로 할당해줌
  // AVFormatContext 구조체에 파일로부터 읽은 컨테이너 정보를 저장
  if (avformat_open_input(&av_format_ctx, argv[1], nullptr, nullptr) < 0) {
    printf("Couldn't open video file\n");
    return 0;
  }

  // AVFormatContext 구조체에 스트림 관련 정보를 읽어서 저장
  if (avformat_find_stream_info(av_format_ctx, nullptr) < 0) {
    printf("Failed to retrieve input stream information\n");
    return 0;
  }

  for (int index = 0; index < av_format_ctx->nb_streams; ++index) {
    // AVCodecParameters 구조체는 스트림에 사용된 코덱 속성에 대한 정보를 가지고 있음
    AVCodecParameters* av_codec_params = av_format_ctx->streams[index]->codecpar;

    if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
      printf("-------- video info --------\n");
      printf("codec_id : %d\n", av_codec_params->codec_id);
      printf("bitrate : %lld\n", av_codec_params->bit_rate);
      printf("width : %d, height : %d\n", av_codec_params->width, av_codec_params->height);
    } else if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
      printf("-------- audio info --------\n");
      printf("codec_id : %d\n", av_codec_params->codec_id);
      printf("bitrate : %lld\n", av_codec_params->bit_rate);
      printf("sample_rate : %d\n", av_codec_params->sample_rate);
      printf("number of channels : %d\n", av_codec_params->channels);
    }
  }

  // AVFormatContext 구조체에 할당한 메모리 해제
  avformat_close_input(&av_format_ctx);

  return 0;
}