# ffmpeg-study


## FFmpeg
- 크로스 플랫폼을 지원하는 오픈소스 멀티미디어 프레임워크
- 비디오, 오디오, 이미지를 쉽게 인코딩/디코딩, 먹싱/디먹싱 할 수 있는 기능을 제공

## License
- 빌드 방법과 외부 라이브러리에 따라 GPL-2.0, GPL-3.0, LGPL-3.0이 적용됨

## FFmpeg Library
- libavcodec : 오디오/비디오의 인코더/디코더 기능 제공
- libavformat : 파일 읽기/쓰기, 오디오/비디오 컨테이너 포맷의 먹서/디먹서 기능 제공
- libavutil : 다양한 유틸리티(난수 생성기, 수학 루틴 등) 제공
- libavdevice : 캡처 및 렌더링 기능 제공
- libavfilter : 미디어 필터 기능 제공
- libswscale : 이미지 스케일링, 픽셀 포맷 변환 기능 제공
- libswresample : 오디오 리샘플링 기능 제공

## FFmpeg Data Structure
FFmpeg은 컨테이너(Container), 스트림(Stream), 코덱(Codec), 패킷(Packet), 프레임(Frame)을 관리 및 저장하기 위해 구조체를 사용

### AVFormatContext
- 컨테이너 정보를 가지고 있는 구조체
- 파일로부터 읽은 컨테이너 내용을 저장하거나 새로 생성한 컨테이너를 파일에 쓰기 위한 용도로 사용
- 내부에 상당히 많은 변수들이 들어있는데 읽은 파일에서 사용하는 변수들과 파일을 쓰기 위해 사용하는 변수들이 같이 있기 때문
- AVFormatContext 구조체 내부에는 적어도 하나 이상의 스트림 구조체(AVStream)가 있음

### AVStream
- 스트림 정보를 가지고 있는 구조체
- 시간과 관련된 정보를 가져올 수 있음
- 가장 많이 사용되는 정보는 프레임 레이트(Frame rate)와 타임 베이스(Time base)
- AVStream 구조체 내부에는 사용된 코덱 정보를 가지고 있는 AVCodecParameters 구조체가 있음

### AVCodec
- 특정 코덱에 대한 정보를 가지고 있는 구조체

### AVCodecContext
- 코덱의 작업 결과를 저장하는 구조체
- 인코딩/디코딩 작업(과정)에 필요한 정보를 가지고 있음
- AVCodecContext 구조체 내부에는 사용할 코덱 정보를 가지고 있는 AVCodec 구조체가 있으며 초기화 시에도 AVCodec 구조체가 필요함
- 비디오 코덱의 경우 해상도, 픽셀 포맷과 같은 정보를 가지고 있음
- 오디오 코덱의 경우 샘플레이트, 채널 개수와 같은 정보를 가지고 있음

### AVPacket
- 압축(인코딩)된 비디오/오디오 데이터를 저장하기 위한 구조체

### AVFrame
- 압축 해제(디코딩)된 비디오/오디오 데이터를 저장하기 위한 구조체
