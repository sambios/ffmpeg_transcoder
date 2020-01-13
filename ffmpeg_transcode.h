//
// Created by hsyuan on 2020-01-08.
//

#ifndef FFMPEG_TRANSCODER_FFMPEG_TRANSCODE_H
#define FFMPEG_TRANSCODER_FFMPEG_TRANSCODE_H

#ifdef __cplusplus

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
}

#endif //__cplusplus

#include <iostream>
#include <memory>
#include <list>
#include <unordered_map>

class BMAVTranscode {
public:
    virtual ~BMAVTranscode(){}
    virtual int Init(int src_codec_id, const char* src_codec_name,
                     int dst_codec_id, const char* dst_codec_name, int dst_w, int dst_h, int dst_fps, int dst_bps)=0;
    virtual int InputFrame(AVPacket *input_pkt) = 0;
    virtual AVPacket* GetOutputPacket() = 0;
};

class BMTranscodeSingleton {
    BMTranscodeSingleton();
    ~BMTranscodeSingleton();

    std::unordered_map<BMAVTranscode*, int> _mapTranscodes[3];
    static BMTranscodeSingleton* _instance;

public:
    static BMTranscodeSingleton* Instance();
    static void Destroy();

    BMAVTranscode* TranscodeCreate();
    void TranscodeDestroy(BMAVTranscode* ptr);

};


#endif //FFMPEG_TRANSCODER_FFMPEG_TRANSCODE_H
