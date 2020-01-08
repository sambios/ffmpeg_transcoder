//
// Created by hsyuan on 2020-01-08.
//

#include "ffmpeg_transcode.h"
#include <list>

class FfmpegTranscode:public AVTranscode
{
    AVCodecContext *enc_ctx_{nullptr};
    AVCodecContext *dec_ctx_{nullptr};
    std::list<AVFrame*> frame_cache_;

    AVCodec* get_perfer_encoder_codec(AVCodecID codec_id, const char *codec_name);
    AVCodec* get_perfer_decoder_codec(AVCodecID codec_id, const char *codec_name);

public:
    FfmpegTranscode();
    ~FfmpegTranscode();
    int Init(int src_codec_id, const char* src_codec_name,
                     int dst_codec_id, const char* dst_codec_name, int dst_w, int dst_h, int dst_fps, int dst_bps) override;

    int InputFrame(AVPacket *input_pkt) override;
    AVPacket* GetOutputPacket() override;
};


FfmpegTranscode::FfmpegTranscode() {
    av_register_all();
    avformat_network_init();

}

FfmpegTranscode::~FfmpegTranscode() {
    std::cout << "FfmpegTranscode() dtor..." << std::endl;
    avformat_network_deinit();
}

AVCodec* FfmpegTranscode::get_perfer_encoder_codec(AVCodecID codec_id, const char *codec_name)
{
    AVCodec *codec = nullptr;
    if (codec_name != nullptr && strlen(codec_name) != 0) {
        codec = avcodec_find_encoder_by_name(codec_name);
        if (NULL == codec) {
            codec = avcodec_find_encoder((enum AVCodecID)codec_id);
        }
    }else{
        codec = avcodec_find_encoder(codec_id);
    }

    return codec;
}

AVCodec* FfmpegTranscode::get_perfer_decoder_codec(AVCodecID codec_id, const char *codec_name)
{
    AVCodec *codec = nullptr;
    if (codec_name != nullptr && strlen(codec_name) != 0) {
        codec = avcodec_find_decoder_by_name(codec_name);
        if (NULL == codec) {
            codec = avcodec_find_decoder((enum AVCodecID)codec_id);
        }
    }else{
        codec = avcodec_find_decoder(codec_id);
    }

    return codec;
}


int FfmpegTranscode::Init(int src_codec_id, const char* src_codec_name,
         int dst_codec_id, const char* dst_codec_name, int dst_w, int dst_h, int dst_fps, int dst_bps)
{

    AVCodec *codec = get_perfer_decoder_codec((AVCodecID)src_codec_id, src_codec_name);

    if (NULL == codec) {
        std::cout << "Can't find codec" << std::endl;
        return -1;
    }

    dec_ctx_ = avcodec_alloc_context3(codec);

    if (avcodec_open2(dec_ctx_, codec, NULL) < 0) {
        std::cout << "Can't open decoder" << std::endl;
        return -1;
    }

    // initialize output context
    codec = get_perfer_encoder_codec((AVCodecID)dst_codec_id, dst_codec_name);
    enc_ctx_ = avcodec_alloc_context3(codec);
    if (avcodec_open2(enc_ctx_, codec, NULL) < 0) {
        std::cout << "Can't open encoder" << std::endl;
        return -1;
    }

    // Other parameters

    return 0;

}

int FfmpegTranscode::InputFrame(AVPacket *input_pkt)
{
    int ret = 0;
    ret = avcodec_send_packet(dec_ctx_, input_pkt);
    if (ret < 0){
        std::cout << "avcodec_send_packet() err=" << ret << std::endl;
        return -1;
    }

    while (1) {
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(dec_ctx_, frame);
        if (AVERROR_AGAIN == ret) {
            break;
        }
    }

}

AVPacket* FfmpegTranscode::GetOutputPacket()
{

}

std::shared_ptr<AVTranscode> AVTranscode::create()
{
    return nullptr;
}


