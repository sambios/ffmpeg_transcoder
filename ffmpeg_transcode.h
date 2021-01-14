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
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
}

#endif //__cplusplus

#include <iostream>
#include <memory>
#include <list>
#include <unordered_map>
#include <mutex>


class BMAVTranscode {
public:
    virtual ~BMAVTranscode(){}
    virtual int Init(int src_codec_id, const char* src_codec_name,
                     int dst_codec_id, const char* dst_codec_name,
                     int dst_w, int dst_h, int dst_fps, int dst_bps,
                     bool use_snaphost=false, int snapshot_width=1280, int snapshot_height=720, int seconds=60)=0;
    virtual int InputFrame(AVPacket *input_pkt) = 0;
    virtual AVPacket* GetOutputPacket() = 0;
    virtual int SetSnapshotCallback(std::function<void(AVPacket *pkt)> cb) = 0;
};

class BMTranscodeSingleton {
    BMTranscodeSingleton();
    ~BMTranscodeSingleton();
    int devid_start_;
    int dev_num_;
    std::mutex lock_trans_;
    std::unordered_map<BMAVTranscode*, int> _mapTranscodes[24];
    static BMTranscodeSingleton* _instance;

public:
    static BMTranscodeSingleton* Instance();
    static void Destroy();

    void SetParams(int devid_start, int dev_num);
    BMAVTranscode* TranscodeCreate();
    void TranscodeDestroy(BMAVTranscode* ptr);

};


#endif //FFMPEG_TRANSCODER_FFMPEG_TRANSCODE_H
