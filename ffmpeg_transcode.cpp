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
    std::list<AVPacket*> pkt_cache_;

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

    enc_ctx_->codec_id = (AVCodecID)dst_codec_id;
    enc_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx_->width = 1920;
    enc_ctx_->height = 1080;
    enc_ctx_->bit_rate = dst_bps;
    enc_ctx_->gop_size = 250;

    enc_ctx_->time_base.num = 1;
    enc_ctx_->time_base.den = dst_fps;

    //H264
    //enc_ctx_->me_range = 16;
    //enc_ctx_->max_qdiff = 4;
    //enc_ctx_->qcompress = 0.6;

    enc_ctx_->qmin = 10;
    enc_ctx_->qmax = 51;

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
    // Decode first of all.
    ret = avcodec_send_packet(dec_ctx_, input_pkt);
    if (ret < 0){
        std::cout << "avcodec_send_packet() err=" << ret << std::endl;
        return -1;
    }

    while (1) {
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(dec_ctx_, frame);
        if (AVERROR(EAGAIN) == ret) {
            av_frame_free(&frame);
            break;
        }

        frame_cache_.push_back(frame);
    }

    // Encode frame
    if (frame_cache_.size() > 0) {
        auto frame = frame_cache_.front();
        ret = avcodec_send_frame(enc_ctx_, frame);
        if (ret < 0){
            std::cout << "avcodec_send_frame() err=" << ret << std::endl;
            return -1;
        }
        frame_cache_.pop_front();

        while(1) {
            AVPacket *out_pkt = av_packet_alloc();
            ret = avcodec_receive_packet(enc_ctx_, out_pkt);
            if (ret != 0) {
                av_packet_free(&out_pkt);
                break;
            }

            pkt_cache_.push_back(out_pkt);
        }
    }

    return 0;
}

AVPacket* FfmpegTranscode::GetOutputPacket()
{
    if (pkt_cache_.size() == 0) {
        return nullptr;
    }

    auto pkt = pkt_cache_.front();
    pkt_cache_.pop_front();
    return pkt;
}

std::shared_ptr<AVTranscode> AVTranscode::create()
{
    std::shared_ptr<AVTranscode> ptr = std::make_shared<FfmpegTranscode>();
    return ptr;
}


