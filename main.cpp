#include <iostream>
#include "ffmpeg_transcode.h"

int main(int argc, char *argv[]) {

    AVFormatContext	*pFormatCtx;
    char filepath[] = "/Users/hsyuan/testfiles/yanxi-1080p.264";
    char out_file[] = "cif_output.264";
    FILE *fp = fopen(out_file, "wb+");

    std::shared_ptr<AVTranscode> trandcoder = AVTranscode::create();
    trandcoder->Init(AV_CODEC_ID_H264, NULL, AV_CODEC_ID_H264, NULL, 352, 288, 30, 32<<10);

    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx, filepath, NULL, NULL)!=0){
        printf("Couldn't open input stream, file=%s\n", filepath);
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
                fwrite(cif_h264_pkt->data,  1, cif_h264_pkt->size, fp);
                av_packet_free(&cif_h264_pkt);
            }

        }

        av_packet_unref(packet);
    }

    fclose(fp);

    av_freep(&packet);
    avformat_close_input(&pFormatCtx);

    return 0;
}