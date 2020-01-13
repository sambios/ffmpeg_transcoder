#include <iostream>
#include "ffmpeg_transcode.h"
#include <thread>
#include <signal.h>

int quit_flag = 0;

static int transcode_one(const std::string& input, int w, int h, int fps, int bps, int index)
{
#if USE_OUTPUT_FILE
    char out_file[255];
    sprintf(out_file, "cif_output_%d.264", index);
    FILE *fp = fopen(out_file, "wb+");
#endif

    AVFormatContext	*pFormatCtx;
    std::shared_ptr<BMAVTranscode> trandcoder;
    trandcoder.reset(BMTranscodeSingleton::Instance()->TranscodeCreate());
    trandcoder->Init(AV_CODEC_ID_H264, "", AV_CODEC_ID_H264, "", w, h, fps, bps);

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
    int frame_cnt = 0;
    while(quit_flag != 1) {
        if (av_read_frame(pFormatCtx, packet) < 0){
            break;
        }

        if(packet->stream_index == 0) {

            trandcoder->InputFrame(packet);
            AVPacket *cif_h264_pkt = trandcoder->GetOutputPacket();
            if (cif_h264_pkt) {
#if USE_OUTPUT_FILE
                fwrite(cif_h264_pkt->data,  1, cif_h264_pkt->size, fp);
#endif
                printf("\b\b\b\b\b\b\b\b\b\b\b\b[%2d]%8d", index, frame_cnt);
                av_packet_free(&cif_h264_pkt);
            }
            frame_cnt++;
        }

        av_packet_unref(packet);
    }

#if USE_OUTPUT_FILE
    fclose(fp);
#endif

    av_freep(&packet);
    avformat_close_input(&pFormatCtx);
    return 0;
}

void sighander(int sig)
{
    quit_flag = 1;
}

int main(int argc, char *argv[]) {

    signal(SIGINT, sighander);
    BMTranscodeSingleton::Instance();
    //char filepath[] = "/Users/hsyuan/testfiles/yanxi-1920x1080-4M-15.264";
    //char filepath[] = "rtsp://admin:hk123456@192.168.1.100";
    int run_N = 1;
    char *filepath=argv[1];
    if (argc > 2) {
        run_N = atoi(argv[2]);
    }

    printf("run_N = %d\n", run_N);
    std::thread **threads=new std::thread*[run_N];

    for(int i = 0;i < run_N; i++) {
        int n = i;
        threads[i] = new std::thread([=] {
            transcode_one(filepath, 352, 288, 25, 32<<10, n);
        });
    }

    for(int i = 0;i < run_N; i++) {
        threads[i]->join();
        delete threads[i];
    }
    delete [] threads;

    BMTranscodeSingleton::Destroy();

    return 0;
}
