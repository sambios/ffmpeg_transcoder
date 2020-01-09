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

    // avfilter
    bool m_enableFilter{false};
    AVFilterGraph *filterGraph_{nullptr};
    AVFilterContext *filterContextSrc_{nullptr};
    AVFilterContext *filterContextSink_{nullptr};


    int dst_width_, dst_height_, dst_fps_, dst_bps_;
    AVCodecID dst_codecid_;
    std::string dst_codec_name_;
    AVPixelFormat dst_pixfmt_;

    int init_filter(int w, int h, int pix_fmt, int fps, const std::string &filter_string);
    int create_encoder_from_avframe(const AVFrame* frameinfo);

    AVCodec* get_prefer_encoder_codec(AVCodecID codec_id, const char *codec_name);
    AVCodec* get_prefer_decoder_codec(AVCodecID codec_id, const char *codec_name);

public:
    FfmpegTranscode();
    ~FfmpegTranscode();
    int Init(int src_codec_id, const char* src_codec_name,
                     int dst_codec_id, const char* dst_codec_name,
                     int dst_w, int dst_h, int dst_fps, int dst_bps) override;

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
    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
    }
}

AVCodec* FfmpegTranscode::get_prefer_encoder_codec(AVCodecID codec_id, const char *codec_name)
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

AVCodec* FfmpegTranscode::get_prefer_decoder_codec(AVCodecID codec_id, const char *codec_name)
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

    dst_height_ = dst_h;
    dst_width_ = dst_w;
    dst_fps_ = dst_fps;
    dst_bps_ = dst_bps;
    dst_codecid_ = (AVCodecID)dst_codec_id;
    if (dst_codec_name) {
        dst_codec_name_ = dst_codec_name;
    }

    AVCodec *codec = get_prefer_decoder_codec((AVCodecID)src_codec_id, src_codec_name);

    if (NULL == codec) {
        std::cout << "Can't find codec" << std::endl;
        return -1;
    }

    dec_ctx_ = avcodec_alloc_context3(codec);

    if (avcodec_open2(dec_ctx_, codec, NULL) < 0) {
        std::cout << "Can't open decoder" << std::endl;
        return -1;
    }



    // Other parameters

    return 0;

}

int FfmpegTranscode::init_filter(int width, int height, int pix_fmt, int fps, const std::string &filter_string)
{
    char args[512];

    AVBufferSinkParams *params = NULL;
    const AVFilter *filterSrc = NULL, *filterSink = NULL;
    AVFilterInOut *filterInOutIn = NULL, *filterInOutOut = NULL;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_NONE};
    // Create video filter graph
    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) {
        std::cout << "Cannot alloc filter graph";
        goto cleanup;
    }

    filterSrc = avfilter_get_by_name("buffer");
    filterSink = avfilter_get_by_name("buffersink");
    if (!filterSrc || !filterSink) {
        std::cout << "Cannot get filter";
        goto cleanup;
    }

    filterInOutIn = avfilter_inout_alloc();
    filterInOutOut = avfilter_inout_alloc();
    if (!filterInOutIn || !filterInOutOut) {
        std::cout << "Cannot alloc filter inout";
        goto cleanup;
    }

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             width, height, pix_fmt, 1, fps, 1, 1
    );

    std::cout << args << std::endl;

    if ((avfilter_graph_create_filter(&filterContextSrc_, filterSrc, "in", args, NULL, filterGraph_) < 0)
        || (avfilter_graph_create_filter(&filterContextSink_, filterSink, "out", NULL, params, filterGraph_) < 0)) {
        std::cout << "Cannot create filter";
        av_free(params);
        goto cleanup;
    }
    av_opt_set_int_list(filterContextSink_, "pix_fmts", pix_fmts,
                        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    /* Endpoints for the filter graph. */
    filterInOutOut->name = av_strdup("in");
    filterInOutOut->filter_ctx = filterContextSrc_;
    filterInOutOut->pad_idx = 0;
    filterInOutOut->next = NULL;

    filterInOutIn->name = av_strdup("out");
    filterInOutIn->filter_ctx = filterContextSink_;
    filterInOutIn->pad_idx = 0;
    filterInOutIn->next = NULL;

    if (avfilter_graph_parse(filterGraph_, filter_string.c_str(),
                             filterInOutIn, filterInOutOut, NULL) < 0) {
        std::cout << "Cannot parse filter graph";
        goto cleanup;
    }

    if (avfilter_graph_config(filterGraph_, NULL) < 0) {
        std::cout << "Cannot config filter graph";
        goto cleanup;
    }

    return 0;

    cleanup:
    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = NULL;
    }
    return -1;
}


int FfmpegTranscode::create_encoder_from_avframe(const AVFrame *frame)
{
    AVCodec *codec = nullptr;
    dst_pixfmt_ = (AVPixelFormat)frame->format;
    // initialize output context
    codec = get_prefer_encoder_codec((AVCodecID)dst_codecid_, dst_codec_name_.c_str());

    enc_ctx_ = avcodec_alloc_context3(codec);

    enc_ctx_->codec_id = dst_codecid_;
    enc_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx_->pix_fmt = dst_pixfmt_;
    enc_ctx_->width = frame->width;
    enc_ctx_->height = frame->height;
    enc_ctx_->bit_rate = dst_bps_;
    enc_ctx_->gop_size = 250;

    enc_ctx_->time_base.num = 1;
    enc_ctx_->time_base.den = dst_fps_;

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
        AVFrame *frame1 = av_frame_alloc();
        ret = avcodec_receive_frame(dec_ctx_, frame1);
        if (AVERROR(EAGAIN) == ret) {
            av_frame_free(&frame1);
            break;
        }


        if (dst_width_ != frame1->width || dst_height_ != frame1->height) {
            if (filterGraph_ == nullptr) {
                // Change resolution, must be resize
                char filter_desc[256];
                sprintf(filter_desc, "scale=%d:%d", dst_width_, dst_height_);
                init_filter(frame1->width, frame1->height, frame1->format, 25, filter_desc);
            }
        }

        if (filterGraph_) {
            av_buffersrc_write_frame(filterContextSrc_, frame1);
            AVFrame *frame2 = av_frame_alloc();
            av_buffersink_get_frame(filterContextSink_, frame2);
            frame_cache_.push_back(frame2);
            av_frame_free(&frame1);
        }else{
            frame_cache_.push_back(frame1);
        }
    }

    // Encode frame
    if (frame_cache_.size() > 0) {
        auto frame = frame_cache_.front();

        if (nullptr == enc_ctx_) {
            create_encoder_from_avframe(frame);
        }

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


