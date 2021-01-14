#include <iostream>
#include "ffmpeg_transcode.h"
#include <thread>
#include <signal.h>
#include "otl_cmd_parser.h"

int quit_flag = 0;
int loop_flag = 0;

static int transcode_one(const std::string& input, int w, int h, int fps, int bps, int max_frame, int index, bool use_snaphost,
        int snapshot_w, int snapshot_h, double *p_fps)
{
#if USE_OUTPUT_FILE
    char out_file[255];
    sprintf(out_file, "cif_output_%d.264", index);
    FILE *fp = fopen(out_file, "wb+");
#endif

    AVFormatContext	*pFormatCtx;
    auto trandcoder = BMTranscodeSingleton::Instance()->TranscodeCreate();
    trandcoder->Init(AV_CODEC_ID_H264, "", AV_CODEC_ID_H264, "", w, h, fps, bps, use_snaphost, snapshot_w, snapshot_h, 60);
    trandcoder->SetSnapshotCallback([index](AVPacket *pkt){
        char filepath[255];
        time_t tnow;
        time(&tnow);
        struct tm* tp = localtime(&tnow);
        sprintf(filepath, "snapshot%d-%d%.2d%.2d%.2d%.2d%.2d.jpg", index, tp->tm_year+1900, tp->tm_mon+1, tp->tm_mday,
                tp->tm_hour, tp->tm_min, tp->tm_sec);

        FILE *fp = fopen(filepath, "wb+");
        fwrite(pkt->data, 1, pkt->size, fp);
        fclose(fp);
    });
    
    loop_flag = (-1==max_frame);
    
LoopStart:
    pFormatCtx = avformat_alloc_context();
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    if(avformat_open_input(&pFormatCtx, input.c_str(), NULL, &opts)!=0){
        printf("Couldn't open input stream, file=%s\n", input.c_str());
        return -1;
    }
   
    av_dict_free(&opts);

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

        if (0 != packet->stream_index){
            av_packet_unref(packet);
            continue;
        }

        packet->pts = (1.0/30)*90*frame_cnt;
        if(packet->stream_index == 0) {
            trandcoder->InputFrame(packet);
            AVPacket *cif_h264_pkt = trandcoder->GetOutputPacket();
            if (cif_h264_pkt) {
#if USE_OUTPUT_FILE
                fwrite(cif_h264_pkt->data,  1, cif_h264_pkt->size, fp);
#endif
                av_packet_free(&cif_h264_pkt);
            }
            frame_cnt++;
        }
        av_packet_unref(packet);
       if (max_frame > 0 && frame_cnt > max_frame) break;
       //av_usleep(20000);
    }
    int64_t delta_time = av_gettime()-start_time;
    *p_fps = (double)frame_cnt*1000000/delta_time;
#if USE_OUTPUT_FILE
    fclose(fp);
#endif
    av_packet_free(&packet);
    avformat_close_input(&pFormatCtx);
    
    if (loop_flag) goto LoopStart;

    return 0;
}

void sighander(int sig)
{
    quit_flag = 1;
    loop_flag = 0;
}

int main(int argc, char *argv[]) {

    int run_N = 1;
    CommandLineParser parser(argc, argv);
    parser.SetFlag("inputfile", "/home/yuan/wkc.264", "Input Filepath");
    parser.SetFlag("width", "352", "Width of Picture");
    parser.SetFlag("height", "288", "Height of Picture");
    parser.SetFlag("bps", "32000", "bitrate");
    parser.SetFlag("fps", "25", "framerate");
    parser.SetFlag("num", "17", "number of channels");
    parser.SetFlag("devid_start", "0", "device id start number");
    parser.SetFlag("dev_num", "1", "device num");
    parser.SetFlag("max_frame", "2000", "max frame to decode");
    parser.SetFlag("use_snapshot", "false", "enable snapshot function");
    parser.SetFlag("snapshot_width", "1280", "snapshot width");
    parser.SetFlag("snapshot_height", "720", "snapshot height");

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
    bool use_snapshot = parser.GetFlag("use_snapshot") == "true";
    int snapshot_w = std::stoi(parser.GetFlag("snapshot_width"));
    int snapshot_h = std::stoi(parser.GetFlag("snapshot_height"));

    signal(SIGINT, sighander);
    auto transcodeModule = BMTranscodeSingleton::Instance();
    transcodeModule->SetParams(dev_id, dev_num);


    printf("run_N = %d\n", run_N);
    std::thread **threads=new std::thread*[run_N];
    double *channel_fps = new double[run_N];

    for(int i = 0;i < run_N; i++) {
	    int n = i;
	    threads[i] = new std::thread([=] {
			    transcode_one(filepath, width, height, fps, bitrate, max_frame, n, use_snapshot, snapshot_w, snapshot_h, &channel_fps[n]);
			    });
    }

    for(int i = 0;i < run_N; i++) {
	    threads[i]->join();
	    delete threads[i];
            printf("%d %.0f\n", i, channel_fps[i]);

    }
    delete [] threads;
    delete [] channel_fps;

    BMTranscodeSingleton::Destroy();
    printf("Transcode Done!\n");
    return 0;
}
