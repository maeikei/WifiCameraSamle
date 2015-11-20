//
//  WifiCameraUtility.m
//  WifiCameraSample
//
//  Created by yma on 2015/11/19.
//  Copyright © 2015年 yma@asra. All rights reserved.
//

#import "WifiCameraUtility.h"
#import <opencv2/opencv.hpp>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include <iostream>



/*
 const static std::tuple<std::string, std::string,bool> constConnectRC[] =
 {
 // rtsp of sjcam.
 std::make_tuple("192.168.1.", "rtsp://192.168.1.254/sjcam.mov",false),
 // udp of gopro.
 std::make_tuple("10.5.5.", "udp://10.5.5.9:8554",true)
 };
 */


#define DUMP_VAR(x) \
printf("%d: %s=<%d>\n",__LINE__,#x,x);

#define HEX_VAR(x) \
printf("%d: %s=<0x%x>\n",__LINE__,#x,x);

#define DUMP_STR_VAR(x)\
printf("%d: %s=<%s>\n",__LINE__,#x,x);



@implementation WifiCameraUtility

@synthesize delegate_;




- (void) setDelegate:(id)delegate
{
    delegate_ = delegate;
}

- (void) open
{
    AVFormatContext* context = avformat_alloc_context();
    
    HEX_VAR(context);
    
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    
    
    
    {
#if 0
        auto ret = avformat_open_input(&context, "rtsp://192.168.1.254/sjcam.mov",NULL,NULL);
#else
        [NSThread detachNewThreadSelector:@selector(threadKeepLive) toTarget:self withObject:nil];
        auto ret = avformat_open_input(&context, "udp://10.5.5.9:8554",NULL,NULL);
#endif
        DUMP_VAR(ret);
        if( ret != 0){
            return ;
        }
    }
    
    auto video_stream_index = av_find_best_stream(context,AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    DUMP_VAR(video_stream_index);
    if( video_stream_index < 0){
        return ;
    }
    auto origin_ctx = context->streams[video_stream_index]->codec;
    DUMP_VAR(origin_ctx->codec_id);

    auto codec = avcodec_find_decoder(origin_ctx->codec_id);
    HEX_VAR(codec);
    DUMP_STR_VAR(origin_ctx->codec_name);
    DUMP_VAR(origin_ctx->pix_fmt);
    DUMP_VAR(origin_ctx->width);
    DUMP_VAR(origin_ctx->height);
    
    auto ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        HEX_VAR(ctx);
        return;
    }
    {
        auto ret = avcodec_copy_context(ctx, origin_ctx);
        if( ret != 0){
            return ;
        }
    }

    {
        auto ret = avcodec_open2(ctx, codec, NULL);
        if( ret != 0){
            return ;
        }
    }
   

    
    
    AVPacket packet;
    av_init_packet(&packet);
    av_read_play(context);//play RTSP
    
    
    auto fr = av_frame_alloc();
    if (!fr) {
        HEX_VAR(fr);
        return;
    }
    auto frBGR = av_frame_alloc();
    if (!frBGR) {
        HEX_VAR(frBGR);
        return;
    }
    auto sizeRead = true;
    
    uint8_t * convBuffer = nullptr;
    
//    SwsContext *img_convert_ctx;
    while(av_read_frame(context,&packet)>=0){//read 100 frames
        if(packet.stream_index == video_stream_index){//packet is video
           
            int got_picture_ptr = -1;
            auto bytes = avcodec_decode_video2(ctx,fr,&got_picture_ptr,&packet);
            DUMP_VAR(bytes);
            DUMP_VAR(got_picture_ptr);
            if ( 0>= bytes) {
                // codec error
                continue;
            }
            if ( 0 >= fr->width || 0 >= fr->height) {
                // codec error
                continue;
            }
            if (sizeRead) {
                auto dstBytes=avpicture_get_size(PIX_FMT_RGB24, fr->width, fr->height);
                convBuffer =(uint8_t *) av_malloc(dstBytes*sizeof(uint8_t));
                avpicture_fill((AVPicture *)frBGR, convBuffer, PIX_FMT_RGB24,
                               fr->width, fr->height);
                sizeRead = false;
            }
            DUMP_VAR(fr->channels);
            DUMP_VAR(fr->height);
            DUMP_VAR(fr->width);
            DUMP_VAR(fr->format);
            DUMP_VAR(fr->sample_rate);
            DUMP_VAR(ctx->pix_fmt);
            DUMP_VAR(ctx->delay);
            static auto img_convert_ctx = sws_getContext(fr->width,fr->height,ctx->pix_fmt,fr->width,fr->height,PIX_FMT_RGB24,SWS_BICUBIC,nullptr,nullptr,nullptr);
            sws_scale(img_convert_ctx,fr->data,fr->linesize,0,fr->height,frBGR->data, frBGR->linesize);
            cv::Mat mat(fr->height, fr->width, CV_8UC3, frBGR->data[0], frBGR->linesize[0]);
            DUMP_VAR(mat.rows);
            DUMP_VAR(mat.cols);
            DUMP_VAR(mat.channels());
            auto uiImg = [self imageWithCVMat:mat];
            DUMP_VAR(delegate_);
            [delegate_ updateFrame:(uiImg)];
            CGImageRef ref = [uiImg CGImage];
            CGImageRelease(ref);
        }
        av_free_packet(&packet);
        av_init_packet(&packet);
    }
    if (convBuffer) {
        av_free(convBuffer);
    }
    av_frame_free(&fr);
    avcodec_free_context(&ctx);
}

static std::vector<uint8_t *> gBuffer;
static const int iConstBufferMax = 30;
static int iBufferCounter = 0;

- (UIImage *)imageWithCVMat:(const cv::Mat&)cvMat{
    auto size = cvMat.elemSize() * cvMat.total();
    NSData *data = [NSData dataWithBytes:cvMat.data length:size];
    DUMP_VAR(data.length);

    
    CGColorSpaceRef colorSpace;
    
    if (cvMat.elemSize() == 1) {
        colorSpace = CGColorSpaceCreateDeviceGray();
    } else {
        colorSpace = CGColorSpaceCreateDeviceRGB();
    }
    
    
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
 
    
    CGImageRef imageRef = CGImageCreate(cvMat.cols,                                     // Width
                                        cvMat.rows,                                     // Height
                                        8,                                              // Bits per component
                                        8 * cvMat.elemSize(),                           // Bits per pixel
                                        cvMat.step[0],                                  // Bytes per row
                                        colorSpace,                                     // Colorspace
                                        kCGImageAlphaNone | kCGBitmapByteOrderDefault,  // Bitmap info flags
                                        provider,                                       // CGDataProviderRef
                                        NULL,                                           // Decode
                                        false,                                          // Should interpolate
                                        kCGRenderingIntentDefault);                     // Intent
    
    UIImage *image = [[UIImage alloc] initWithCGImage:imageRef];
    CGImageRelease(imageRef);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
    return image;
}

-(void)threadKeepLive
{
    NSURL *url = [NSURL URLWithString:@"http://10.5.5.9/gp/gpControl/execute?p1=gpStream&c1=restart"];
    DUMP_VAR(url);
    NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config];
    NSURLSessionDataTask* task =[session dataTaskWithURL:url];
    for (;;){
        [task resume];
        [NSThread sleepForTimeInterval:10.0];
        DUMP_VAR(url);
    }
}

@end