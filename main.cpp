#include <iostream>
#include "ffmpeg_transcode.h"
#include <thread>
#include <signal.h>
#include "otl_cmd_parser.h"

int quit_flag = 0;

static int transcode_one(const std::string& input, int w, int h, int fps, int bps, int max_frame, int index)
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
    int64_t start_time = av_gettime();
    while(quit_flag != 1) {
        if (av_read_frame(pFormatCtx, packet) < 0){
            break;
        }
        packet->pts = (1.0/30)*90*frame_cnt;
        if(packet->stream_index == 0) {
            trandcoder->InputFrame(packet);
            AVPacket *cif_h264_pkt = trandcoder->GetOutputPacket();
            if (cif_h264_pkt) {
#if USE_OUTPUT_FILE
                fwrite(cif_h264_pkt->data,  1, cif_h264_pkt->size, fp);
#endif
                //printf("\b\b\b\b\b\b\b\b\b\b\b\b[%2d]%8d", index, frame_cnt);
                av_packet_free(&cif_h264_pkt);
            }
            frame_cnt++;
        }
        av_packet_unref(packet);
       if (frame_cnt > max_frame) break;
    }
    int64_t delta_time = av_gettime()-start_time;
    printf("%d, fps=%f\n", (double)frame_cnt*1000000/delta_time);
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

    int run_N = 1;
    CommandLineParser parser(argc, argv);
    parser.SetFlag("inputfile", "", "Input Filepath");
    parser.SetFlag("width", "352", "Width of Picture");
    parser.SetFlag("height", "288", "Height of Picture");
    parser.SetFlag("bps", "32000", "bitrate");
    parser.SetFlag("fps", "25", "framerate");
    parser.SetFlag("num", "1", "number of channels");
    parser.SetFlag("devid_start", "3", "device id start number");
    parser.SetFlag("dev_num", "1", "device num");
    parser.SetFlag("max_frame", "2000", "max frame to decode");

    parser.ProcessFlags();
    parser.PrintEnteredFlags();
    std::string filepath = parser.GetFlag("inputfile");
    int width = std::stoi(parser.GetFlag("width"));
    int height = std::stoi(parser.GetFlag("height"));
    run_N = std::stoi(parser.GetFlag("num"));
    int fps = std::stoi(parser.GetFlag("fps"));
    int bitrate = std::stoi(parser.GetFlag("bps"));
    int dev_id = std::stoi(parser.GetFlag("devid_start"));
    int dev_num = std::stoi(parser.GetFlag("dev_num"));
    int max_frame = std::stoi(parser.GetFlag("max_frame"));

    signal(SIGINT, sighander);
    auto transcodeModule = BMTranscodeSingleton::Instance();
    transcodeModule->SetParams(dev_id, dev_num);
    //char filepath[] = "/Users/hsyuan/testfiles/yanxi-1920x1080-4M-15.264";
    //char filepath[] = "rtsp://admin:hk123456@192.168.1.100";


    printf("run_N = %d\n", run_N);
    std::thread **threads=new std::thread*[run_N];

    for(int i = 0;i < run_N; i++) {
        int n = i;
        threads[i] = new std::thread([=] {
            transcode_one(filepath, width, height, fps, bitrate, max_frame, n);
        });
    }

    for(int i = 0;i < run_N; i++) {
        threads[i]->join();
        delete threads[i];
    }
    delete [] threads;

    BMTranscodeSingleton::Destroy();

    getchar();

    return 0;
}
