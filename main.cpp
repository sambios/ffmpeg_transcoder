#include <iostream>
#include "ffmpeg_transcode.h"

int main(int argc, char *argv[]) {

    AVFormatContext	*pFormatCtx;
    char filepath[] = "/Users/hsyuan/testfiles/";

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


        }

        av_packet_unref(packet);
    }

    printf("\n");

    av_freep(&packet);
    avformat_close_input(&pFormatCtx);

    return 0;
}