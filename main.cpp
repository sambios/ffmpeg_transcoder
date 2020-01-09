#include <iostream>
#include "ffmpeg_transcode.h"
#include <thread>

static int transcode_one(const std::string& input, int w, int h, int fps, int bps, int index)
{
    char out_file[255];
    sprintf(out_file, "cif_output_%d.264", index);
    //FILE *fp = fopen(out_file, "wb+");

    AVFormatContext	*pFormatCtx;
    std::shared_ptr<AVTranscode> trandcoder = AVTranscode::create();
    trandcoder->Init(AV_CODEC_ID_H264, NULL, AV_CODEC_ID_H264, NULL, w, h, fps, bps);

    pFormatCtx = avformat_alloc_context();

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    if(avformat_open_input(&pFormatCtx, input.c_str(), NULL, &opts)!=0){
        printf("Couldn't open input stream, file=%s\n", input.c_str());
        return -1;
    }

    if(avformat_find_stream_info(pFormatCtx,NULL)<0){
        printf("Couldn't find stream information.\n");
        return -1;
    }

    AVPacket *packet = av_packet_alloc();

    while(1) {
        if (av_read_frame(pFormatCtx, packet) < 0){
            break;
        }

        if(packet->stream_index == 0) {

            trandcoder->InputFrame(packet);
            AVPacket *cif_h264_pkt = trandcoder->GetOutputPacket();
            if (cif_h264_pkt) {
                //fwrite(cif_h264_pkt->data,  1, cif_h264_pkt->size, fp);
                av_packet_free(&cif_h264_pkt);
            }

        }

        av_packet_unref(packet);
    }

    //fclose(fp);

    av_freep(&packet);
    avformat_close_input(&pFormatCtx);
    return 0;
}

#define N 1

int main(int argc, char *argv[]) {


    //char filepath[] = "/Users/hsyuan/testfiles/yanxi-1920x1080-4M-15.264";
    char filepath[] = "rtsp://admin:hk123456@192.168.1.100";
    std::thread *threads[N];

    for(int i = 0;i < N; i++) {
        threads[i] = new std::thread([&] {
            transcode_one(filepath, 352, 288, 25, 32<<10, i);
        });
    }

    for(int i = 0;i < N; i++) {
        threads[i]->join();
        delete threads[i];
    }

    return 0;
}