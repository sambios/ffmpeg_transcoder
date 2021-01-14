//
// Created by hsyuan on 2020-01-08.
//

#include "ffmpeg_transcode.h"
#include <list>
#include <assert.h>

typedef struct FfmpegFilterContext{
    AVFilterGraph *filterGraph{nullptr};
    AVFilterContext *filterContextSrc{nullptr};
    AVFilterContext *filterContextSink{nullptr};
}FfmpegFilterContext;

class FfmpegTranscode:public BMAVTranscode
{
    AVCodecContext *enc_ctx_{nullptr};
    AVCodecContext *dec_ctx_{nullptr};
    AVCodecContext *jpeg_enc_ctx_ {nullptr};

    std::list<AVFrame*> frame_cache_;
    std::list<AVPacket*> pkt_cache_;

    //
    FfmpegFilterContext filter_scale_jpeg_;
    FfmpegFilterContext filter_scale_cif_;

    AVBufferRef*  hw_device_ctx_{nullptr};

    int dst_width_, dst_height_, dst_fps_, dst_bps_;
    AVCodecID dst_codecid_;
    std::string dst_codec_name_;
    AVPixelFormat dst_pixfmt_;
    std::string devname_;

    //Snapshot
    bool use_snapshot_{false};
    int snapshot_width_{0},snapshot_height_{0};
    int snapshot_delay_{300};
    std::function<void(AVPacket *pkt)> snapshot_cb_;
    time_t last_snapshot_time_{0};

    int init_filter(FfmpegFilterContext *filter, int w, int h, int pix_fmt, int fps, const std::string &filter_string);
    int create_encoder_from_avframe(const AVFrame* frameinfo);
    int create_jpeg_encoder(int width, int height, AVPixelFormat pixfmt);
    int jpeg_decode(AVFrame* frame);

    AVCodec* get_prefer_encoder_codec(AVCodecID codec_id, const char *codec_name);
    AVCodec* get_prefer_decoder_codec(AVCodecID codec_id, const char *codec_name);
    static enum AVPixelFormat get_bmcodec_format(AVCodecContext *ctx,
                                          const enum AVPixelFormat *pix_fmts);
public:
    FfmpegTranscode(const char* dev_name);
    ~FfmpegTranscode();

    int Init(int src_codec_id, const char *src_codec_name,
             int dst_codec_id, const char *dst_codec_name,
             int dst_w, int dst_h, int dst_fps, int dst_bps,
             bool use_snaphost = false, int snapshot_width = 0, int snapshot_height = 0,
             int seconds = 300) override;

    int InputFrame(AVPacket *input_pkt) override;
    AVPacket* GetOutputPacket() override;
    int SetSnapshotCallback(std::function<void(AVPacket *pkt)> cb) override {
        snapshot_cb_ = cb;
        return 0;
    };
};

FfmpegTranscode::FfmpegTranscode(const char* dev_name) {
    av_register_all();
    devname_ = dev_name;
}

FfmpegTranscode::~FfmpegTranscode() {
    std::cout << "FfmpegTranscode() dtor..." << std::endl;
    if (enc_ctx_) {
#if USE_BMCODEC
        if (enc_ctx_->hw_device_ctx) {
            av_buffer_unref(&enc_ctx_->hw_device_ctx);
        }
#endif
        avcodec_close(enc_ctx_);
        avcodec_free_context(&enc_ctx_);
    }

    if (dec_ctx_) {
        avcodec_close(dec_ctx_);
        avcodec_free_context(&dec_ctx_);
    }

    if (jpeg_enc_ctx_) {
#if USE_BMCODEC
        if (jpeg_enc_ctx_->hw_device_ctx) {
            av_buffer_unref(&jpeg_enc_ctx_->hw_device_ctx);
        }
#endif
        avcodec_close(jpeg_enc_ctx_);
        avcodec_free_context(&jpeg_enc_ctx_);
    }

    if (filter_scale_jpeg_.filterGraph) {
        avfilter_graph_free(&filter_scale_jpeg_.filterGraph);
    }

    if (filter_scale_cif_.filterGraph) {
        avfilter_graph_free(&filter_scale_cif_.filterGraph);
    }
    
    if (hw_device_ctx_) {
       av_buffer_unref(&hw_device_ctx_);
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

enum AVPixelFormat FfmpegTranscode::get_bmcodec_format(AVCodecContext *ctx,
                                             const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;
#if USE_BMCODEC
    av_log(ctx, AV_LOG_TRACE, "[%s,%d] Try to get HW surface format.\n", __func__, __LINE__);

    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_BMCODEC) {
            AVHWFramesContext  *frames_ctx;
            int ret;
                       
            /* create a pool of surfaces to be used by the decoder */
            ctx->hw_frames_ctx = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
            if (!ctx->hw_frames_ctx)
                return AV_PIX_FMT_NONE;
            frames_ctx   = (AVHWFramesContext*)ctx->hw_frames_ctx->data;

            frames_ctx->format            = AV_PIX_FMT_BMCODEC;
            frames_ctx->sw_format         = ctx->sw_pix_fmt;

             if (ctx->coded_width > 0)
                frames_ctx->width         = FFALIGN(ctx->coded_width, 32);
            else if (ctx->width > 0)
                frames_ctx->width         = FFALIGN(ctx->width, 32);
            else
                frames_ctx->width         = FFALIGN(1920, 32);

            if (ctx->coded_height > 0)
                frames_ctx->height        = FFALIGN(ctx->coded_height, 32);
            else if (ctx->height > 0)
                frames_ctx->height        = FFALIGN(ctx->height, 32);
            else
                frames_ctx->height        = FFALIGN(1088, 32);

            frames_ctx->initial_pool_size = 0; // Don't prealloc pool.

            ret = av_hwframe_ctx_init(ctx->hw_frames_ctx);
            if (ret < 0)
                goto failed;

            av_log(ctx, AV_LOG_TRACE, "[%s,%d] Got HW surface format:%s.\n",
                   __func__, __LINE__, av_get_pix_fmt_name(AV_PIX_FMT_BMCODEC));

            return AV_PIX_FMT_BMCODEC;
        }
    }

    failed:
    av_log(ctx, AV_LOG_ERROR, "Unable to decode this file using BMCODEC.\n");
#endif
    return AV_PIX_FMT_NONE;
}

int FfmpegTranscode::Init(int src_codec_id, const char* src_codec_name,
         int dst_codec_id, const char* dst_codec_name, int dst_w, int dst_h, int dst_fps, int dst_bps,
                          bool use_snaphost, int snapshot_width, int snapshot_height, int seconds)
{
    int ret = 0;
#if USE_BMCODEC
    ret = av_hwdevice_ctx_create(&hw_device_ctx_, AV_HWDEVICE_TYPE_BMCODEC, devname_.c_str(), NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to create a BMCODEC device. Error code: %s\n", av_err2str(ret));
    }
#endif
    dst_height_ = dst_h;
    dst_width_ = dst_w;
    dst_fps_ = dst_fps;
    dst_bps_ = dst_bps;
    dst_codecid_ = (AVCodecID)dst_codec_id;
    if (dst_codec_name) {
        dst_codec_name_ = dst_codec_name;
    }

    use_snapshot_ = use_snaphost;
    snapshot_height_ = snapshot_height;
    snapshot_width_ = snapshot_width;
    snapshot_delay_ = seconds;

    AVCodec *codec = get_prefer_decoder_codec((AVCodecID)src_codec_id, src_codec_name);

    if (NULL == codec) {
        std::cout << "Can't find codec" << std::endl;
        return -1;
    }

    dec_ctx_ = avcodec_alloc_context3(codec);
#if USE_BMCODEC
    if (hw_device_ctx_) {
        dec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
        dec_ctx_->get_format = get_bmcodec_format;
    }
#endif
    dec_ctx_->width = 1920;
    dec_ctx_->height = 1080;

    AVDictionary *opts = NULL;
    dec_ctx_->thread_count = 1;
    //dec_ctx_->thread_type=FF_THREAD_FRAME;

    av_dict_set(&opts, "sophon_idx", devname_.c_str(), 0);
    av_dict_set_int(&opts, "extra_frame_buffer_num", 2, 0);
    //av_dict_set_int(&opts, "cbcr_interleave", 0, 0);
    av_dict_set_int(&opts, "cbcr_interleave", 1, 0);
    /* To fix the memory waste, make video decoder to output compressed frame buffer */
    av_dict_set_int(&opts, "output_format", 101, 0);

    if (avcodec_open2(dec_ctx_, codec, &opts) < 0) {
        ret = -1;
        std::cout << "Can't open decoder" << std::endl;
        goto end;
    }

end:
    av_dict_free(&opts);
    return ret;
}

int FfmpegTranscode::init_filter(FfmpegFilterContext *filter, int width, int height, int pix_fmt, int fps, const std::string &filter_string)
{
    char args[512];
    int ret;
    AVBufferSinkParams *params = NULL;
    AVBufferSrcParameters par={0};

    const AVFilter *filterSrc = NULL, *filterSink = NULL;
    AVFilterInOut *filterInOutIn = NULL, *filterInOutOut = NULL;
#if USE_BMCODEC
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUV420P,AV_PIX_FMT_NV12, AV_PIX_FMT_BMCODEC, AV_PIX_FMT_NONE};
#else
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUV420P,AV_PIX_FMT_NV12, AV_PIX_FMT_NONE};
#endif
    // Create video filter graph
    filter->filterGraph = avfilter_graph_alloc();
    if (!filter->filterGraph) {
        std::cout << "Cannot alloc filter graph";
        goto End;
    }

    filterSrc = avfilter_get_by_name("buffer");
    filterSink = avfilter_get_by_name("buffersink");
    if (!filterSrc || !filterSink) {
        std::cout << "Cannot get filter";
        goto End;
    }

    filterInOutIn = avfilter_inout_alloc();
    filterInOutOut = avfilter_inout_alloc();
    if (!filterInOutIn || !filterInOutOut) {
        std::cout << "Cannot alloc filter inout";
        goto End;
    }

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             width, height, pix_fmt, 1, fps, 1, 1
    );

    std::cout << "filterSrc:[" << av_get_pix_fmt_name((AVPixelFormat)pix_fmt) << "]" << args << std::endl;

    filter->filterGraph->thread_type=0;
    if ((avfilter_graph_create_filter(&filter->filterContextSrc, filterSrc, "in", args, NULL, filter->filterGraph) < 0)
        || (avfilter_graph_create_filter(&filter->filterContextSink, filterSink, "out", NULL, params, filter->filterGraph) < 0)) {
        std::cout << "Cannot create filter";
        av_free(params);
        goto End;
    }

#if USE_BMCODEC
    par.format = AV_PIX_FMT_NONE;
    par.hw_frames_ctx = dec_ctx_->hw_frames_ctx;
    ret = av_buffersrc_parameters_set(filter->filterContextSrc, &par);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "av_buffersrc_parameters_set failed\n");
        goto End;
    }
#endif

    av_opt_set_int_list(filter->filterContextSink, "pix_fmts", pix_fmts,
                        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    /* Endpoints for the filter graph. */
    filterInOutOut->name = av_strdup("in");
    filterInOutOut->filter_ctx = filter->filterContextSrc;
    filterInOutOut->pad_idx = 0;
    filterInOutOut->next = NULL;

    filterInOutIn->name = av_strdup("out");
    filterInOutIn->filter_ctx = filter->filterContextSink;
    filterInOutIn->pad_idx = 0;
    filterInOutIn->next = NULL;

    if (avfilter_graph_parse(filter->filterGraph, filter_string.c_str(),
                             filterInOutIn, filterInOutOut, NULL) < 0) {
        std::cout << "Cannot parse filter graph";
        goto End;
    }

#if USE_BMCODEC
    if (hw_device_ctx_) {
        for (int i = 0; i < filter->filterGraph->nb_filters; i++) {
            filter->filterGraph->filters[i]->hw_device_ctx = av_buffer_ref(hw_device_ctx_);
            if (!filter->filterGraph->filters[i]->hw_device_ctx) {
                av_log(NULL, AV_LOG_ERROR, "av_buffer_ref failed\n");
                goto End;
            }
        }
    }
#endif

    if (avfilter_graph_config(filter->filterGraph, NULL) < 0) {
        std::cout << "Cannot config filter graph";
        goto End;
    }

    return 0;
    End:
    if (filter->filterGraph) {
        avfilter_graph_free(&filter->filterGraph);
    }
    return -1;
}


int FfmpegTranscode::create_encoder_from_avframe(const AVFrame *frame)
{
    AVCodec *codec = nullptr;
    dst_pixfmt_ = (AVPixelFormat)frame->format;
    if(AV_PIX_FMT_YUVJ420P == dst_pixfmt_) {
        dst_pixfmt_ = AV_PIX_FMT_YUV420P;
    }
    // initialize output context
    codec = get_prefer_encoder_codec((AVCodecID)dst_codecid_, dst_codec_name_.c_str());

    enc_ctx_ = avcodec_alloc_context3(codec);

    enc_ctx_->codec_id = dst_codecid_;
    enc_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx_->pix_fmt = dst_pixfmt_;
    enc_ctx_->width = frame->width;
    enc_ctx_->height = frame->height;
    enc_ctx_->bit_rate = dst_bps_;
    enc_ctx_->gop_size = 50;
    enc_ctx_->thread_count=1;
    //enc_ctx_->thread_type=FF_THREAD_FRAME;

    enc_ctx_->time_base.num = 1;
    enc_ctx_->time_base.den = dst_fps_;
    //H264
    //enc_ctx_->me_range = 16;
    //enc_ctx_->max_qdiff = 4;
    //enc_ctx_->qcompress = 0.6;

    enc_ctx_->qmin = 2;
    enc_ctx_->qmax = 51;
    enc_ctx_->rc_buffer_size= (int)enc_ctx_->bit_rate*3;
    enc_ctx_->has_b_frames = 0;
    enc_ctx_->max_b_frames = 0;

#if USE_BMCODEC
    if (dec_ctx_->hw_device_ctx) {
        AVHWFramesContext *frames_ctx;
        enc_ctx_->hw_frames_ctx = av_buffer_ref(dec_ctx_->hw_frames_ctx);
        if (enc_ctx_->hw_frames_ctx == NULL) {
            printf("%s() av_buffer_ref failed\n", __FUNCTION__);
            return -1;
        }

        frames_ctx = (AVHWFramesContext *) enc_ctx_->hw_frames_ctx->data;
        frames_ctx->format = AV_PIX_FMT_BMCODEC;
        /* The output format of scale_bm is yuv420p, not nv12 */
        frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
        frames_ctx->width = FFALIGN(frame->width, 32);
        frames_ctx->height = FFALIGN(frame->height, 32);
        frames_ctx->initial_pool_size = 0; // Don't prealloc pool.

        int ret = av_hwframe_ctx_init(enc_ctx_->hw_frames_ctx);
        if (ret < 0) {
            printf("av_hwframe_ctx_init err=%d\n", ret);
            return ret;
        }
    }
#endif


    
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "sophon_idx", devname_.c_str(), 0);
    av_dict_set(&opts, "gop_preset", "2", 0);
    av_dict_set(&opts, "enc-params", "gop_preset=2:max_qp=44", 0);

    if (avcodec_open2(enc_ctx_, codec, &opts) < 0) {
        av_dict_free(&opts);
        std::cout << "Can't open encoder" << std::endl;
        return -1;
    }
    av_dict_free(&opts);

    return 0;
}

int FfmpegTranscode::create_jpeg_encoder(int width, int height, AVPixelFormat pixfmt)
{
    int ret = 0;
    AVDictionary *opts = NULL;
    AVCodec *jpeg_encoder;
    // initialize jpeg encoder
    jpeg_encoder = get_prefer_encoder_codec(AV_CODEC_ID_MJPEG, "jpeg_bm");
    if (NULL == jpeg_encoder) {
        av_log(NULL, AV_LOG_ERROR, "Could not find encoder 'jpeg_bm'\n");
        goto end;
    }

    jpeg_enc_ctx_ = avcodec_alloc_context3(jpeg_encoder);
    if (jpeg_enc_ctx_ == NULL) {
        av_log(NULL, AV_LOG_ERROR, "avcodec_alloc_context3 failed\n");
        goto end;
    }

#if USE_BMCODEC
    //
    //we need to ref hw_frames_ctx of decoder to initialize encoder's codec.
    // Only after we get a decoded frame, can we obtain its hw_frames_ctx
    //
    if (NULL == jpeg_enc_ctx_->hw_frames_ctx) {
        AVHWFramesContext *hw_frames_ctx=NULL;
        jpeg_enc_ctx_->hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx_);
        if (!jpeg_enc_ctx_->hw_frames_ctx) {
            ret= AVERROR(ENOMEM);
            goto end;
        }

        hw_frames_ctx = (AVHWFramesContext*)jpeg_enc_ctx_->hw_frames_ctx->data;

        hw_frames_ctx->format        = AV_PIX_FMT_BMCODEC;
        hw_frames_ctx->sw_format     = AV_PIX_FMT_YUVJ420P;
        hw_frames_ctx->width         = FFALIGN(width, 32);
        hw_frames_ctx->height        = FFALIGN(height, 32);

        hw_frames_ctx->initial_pool_size = 0; // Don't prealloc pool.

        ret = av_hwframe_ctx_init(jpeg_enc_ctx_->hw_frames_ctx);
        if (ret < 0)
            goto end;
    }
#endif

    jpeg_enc_ctx_->pix_fmt   = pixfmt;
    jpeg_enc_ctx_->width     = width;
    jpeg_enc_ctx_->height    = height;

    jpeg_enc_ctx_->time_base = AVRational{25, 1};
    jpeg_enc_ctx_->framerate = dec_ctx_->framerate;

    jpeg_enc_ctx_->bit_rate_tolerance = dst_bps_;
    jpeg_enc_ctx_->bit_rate = 0;

    // if pix_fmt is YUV420P, must set this.
    jpeg_enc_ctx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    ret = avcodec_open2(jpeg_enc_ctx_, jpeg_encoder, &opts);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open encode codec. Error code: %s\n",
               av_err2str(ret));
        goto end;
    }

    end:
    av_dict_free(&opts);
    return ret;
}

int FfmpegTranscode::jpeg_decode(AVFrame *frame)
{
    int ret = 0;
    AVPacket enc_pkt;

    av_init_packet(&enc_pkt);
    enc_pkt.data = NULL;
    enc_pkt.size = 0;

    ret = avcodec_send_frame(jpeg_enc_ctx_, frame);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error during encoding. Error code: %s\n", av_err2str(ret));
        return ret;
    }

    while (1) {
        ret = avcodec_receive_packet(jpeg_enc_ctx_, &enc_pkt);
        if (ret != 0) break;
        if (enc_pkt.size > 0 && snapshot_cb_ != nullptr) {
            snapshot_cb_(&enc_pkt);
        }
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


        if (true/*dst_width_ != frame1->width || dst_height_ != frame1->height*/) {
            if (filter_scale_cif_.filterGraph == nullptr) {
                // Change resolution, must be resize
                char filter_desc[256];
#ifdef USE_BMCODEC
                sprintf(filter_desc, "scale_bm=%d:%d", dst_width_, dst_height_);
#else
                sprintf(filter_desc, "scale=%d:%d", dst_width_, dst_height_);
#endif
                int pixfmt = frame1->format;
                init_filter(&filter_scale_cif_, frame1->width, frame1->height, pixfmt, 25, filter_desc);


                if (use_snapshot_) {
                    if (snapshot_width_ == 0 || snapshot_height_ == 0) {
                        snapshot_width_ = frame1->width;
                        snapshot_height_ = frame1->height;
                    }
#ifdef USE_BMCODEC
                    sprintf(filter_desc, "scale_bm=%d:%d:format=yuvj420p", snapshot_width_, snapshot_height_);
#else
                    sprintf(filter_desc, "scale=%d:%d,format=yuvj420p", snapshot_width_, snapshot_height_);
#endif
                    init_filter(&filter_scale_jpeg_, frame1->width, frame1->height, pixfmt, 25, filter_desc);
                }

            }
        }

        if (filter_scale_cif_.filterGraph) {
            if (use_snapshot_ && filter_scale_jpeg_.filterGraph) {
                time_t tnow;
                time(&tnow);
                if (tnow - last_snapshot_time_ >= snapshot_delay_ && tnow != last_snapshot_time_) {
                    std::cout << "snapshot " << std::endl;
                    av_buffersrc_write_frame(filter_scale_jpeg_.filterContextSrc, frame1);
                    AVFrame *frame2 = av_frame_alloc();
                    av_buffersink_get_frame(filter_scale_jpeg_.filterContextSink, frame2);
                    if (jpeg_enc_ctx_ == NULL) {
                        create_jpeg_encoder(snapshot_width_, snapshot_height_, (AVPixelFormat)frame2->format);
                    }
                    jpeg_decode(frame2);
                    av_frame_free(&frame2);
                    last_snapshot_time_ = tnow;
                }
            }

            av_buffersrc_write_frame(filter_scale_cif_.filterContextSrc, frame1);
            AVFrame *frame2 = av_frame_alloc();
            av_buffersink_get_frame(filter_scale_cif_.filterContextSink, frame2);
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
        av_frame_free(&frame);

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




BMTranscodeSingleton* BMTranscodeSingleton::_instance = NULL;
BMTranscodeSingleton::BMTranscodeSingleton():devid_start_(0),dev_num_(1) {

}

BMTranscodeSingleton::~BMTranscodeSingleton() {

}

BMTranscodeSingleton* BMTranscodeSingleton::Instance() {
    if (_instance == NULL) {
        _instance = new BMTranscodeSingleton();
    }

    return _instance;
}

void BMTranscodeSingleton::Destroy() {
    if (_instance) {
        _instance->lock_trans_.lock();
        for(int i = 0;i < _instance->dev_num_; ++i) {
            for(auto kv:_instance->_mapTranscodes[i]) {
                delete kv.first;
            }
        }
       _instance->lock_trans_.unlock();
        delete _instance;
        _instance = NULL;
    }
}

void BMTranscodeSingleton::SetParams(int devid_start, int dev_num)
{
   devid_start_ = devid_start;
   dev_num_ = dev_num;
}

BMAVTranscode* BMTranscodeSingleton::TranscodeCreate()
{
    int min_index = 0;
    lock_trans_.lock();
    int last_size = _mapTranscodes[0].size();
    for(int i = 1;i < dev_num_; i ++) {
        if (_mapTranscodes[i].size() < last_size) {
            min_index = i;
            last_size = _mapTranscodes[i].size();
        }
    }
    char dev_name[128];
    sprintf(dev_name, "%d", devid_start_ + min_index);
    auto ptr =  new FfmpegTranscode(dev_name);
    _mapTranscodes[min_index][ptr] = 1;
    lock_trans_.unlock();
    return ptr;
}

void BMTranscodeSingleton::TranscodeDestroy(BMAVTranscode* ptr) {
    if (ptr) {
        lock_trans_.lock();
        for(int i = 0;i < dev_num_; ++i) {
            _mapTranscodes[i].erase(ptr);
        }
        delete ptr;
        lock_trans_.unlock();
    }
}


