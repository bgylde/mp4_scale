#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

#define FREE_MEM(a) if(a){free(a);a=NULL;}
#define LOGE(format, ...) printf("[ERROR][%s:%d] "format, __FILE__, __LINE__, ## __VA_ARGS__)
#define LOGW(format, ...) printf("[WARNING][%s:%d] "format, __FILE__, __LINE__, ## __VA_ARGS__)
#define LOGI(format, ...) printf("[INFO][%s:%d] "format, __FILE__, __LINE__, ## __VA_ARGS__)

typedef struct {
    AVFormatContext     *pFormatCtx;
    AVCodecContext      *pCodecCtx;
    AVCodec             *pCodec;
    AVFrame             *pFrame;
    AVFrame             *pFrameYUV;
    uint8_t             *out_buffer;
    AVPacket            *packet;
    struct SwsContext   *img_convert_ctx;

    char                *mp4_path;
    int                  videoindex;
    int                  frame_cnt;
    clock_t              time_start;
    clock_t              time_finish;
}MP4_DECODE_TAG;

MP4_DECODE_TAG * init_decode_tag(char * mp4_file);
void decode_tag(MP4_DECODE_TAG * context);
void uninit_decode_tag(MP4_DECODE_TAG * context);

int decode(char * input_jstr, char * output_jstr);

int yuv_to_jpeg(const char * out_file, const unsigned char * yuv_data, int width, int height);

int main(int argc, char * argv[])
{
    //decode("/data/temp/duan.mp4", "dota2_output_yuv.yuv");

    MP4_DECODE_TAG * context = NULL;

    context = init_decode_tag("/data/temp/dota2.mp4");
    decode_tag(context);
    uninit_decode_tag(context);
    
    return 0;
}

MP4_DECODE_TAG * init_decode_tag(char * mp4_file)
{
    int i = -1;
    MP4_DECODE_TAG * context = NULL;

    if(NULL == mp4_file)
    {
        LOGE("mp4_file is NULL\n");
        return context;
    }
    
    context = (MP4_DECODE_TAG *)malloc(sizeof(MP4_DECODE_TAG));
    if(NULL == context)
    {
        LOGE("malloc error: %s\n", strerror(errno));
        return context;
    }
    
    memset(context, 0, sizeof(MP4_DECODE_TAG));
    context->pFormatCtx         = NULL;
    context->pCodecCtx          = NULL;
    context->pCodec             = NULL;
    context->pFrame             = NULL;
    context->pFrameYUV          = NULL;
    context->out_buffer         = NULL;
    context->packet             = NULL;
    context->img_convert_ctx    = NULL;
    context->mp4_path           = NULL;

    context->mp4_path           = strdup(mp4_file);
    context->videoindex         = -1;
    context->frame_cnt          = 0;
    
    av_register_all();
    avformat_network_init();
    context->pFormatCtx = avformat_alloc_context();
    if(avformat_open_input(&(context->pFormatCtx), context->mp4_path, NULL, NULL) != 0)
    {
        LOGE("Couldn't open input stream.\n");
        uninit_decode_tag(context);
    }
    
    if(avformat_find_stream_info(context->pFormatCtx, NULL) < 0)
    {
        LOGE("Couldn't find stream information.\n");
        uninit_decode_tag(context);
    }

    for(i = 0; i < context->pFormatCtx->nb_streams; i++)
    {
        if(context->pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            context->videoindex = i;
            break;
        }
    }
    
    if(context->videoindex == -1)
    {
        LOGE("Couldn't find a video stream.\n");
        uninit_decode_tag(context);
    }
    
    context->pCodecCtx = context->pFormatCtx->streams[context->videoindex]->codec;
    context->pCodec = avcodec_find_decoder(context->pCodecCtx->codec_id);
    if(context->pCodec == NULL)
    {
        LOGE("Couldn't find Codec.\n");
        uninit_decode_tag(context);
    }
    
    if(avcodec_open2(context->pCodecCtx, context->pCodec, NULL) < 0)
    {
        LOGE("Couldn't open codec.\n");
        uninit_decode_tag(context);
    }

    context->pFrame = av_frame_alloc();
    context->pFrameYUV = av_frame_alloc();
    context->out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, context->pCodecCtx->width, context->pCodecCtx->height, 1));
    av_image_fill_arrays(context->pFrameYUV->data, context->pFrameYUV->linesize, context->out_buffer,
                         AV_PIX_FMT_YUV420P, context->pCodecCtx->width, context->pCodecCtx->height, 1);

    context->packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    context->img_convert_ctx = sws_getContext(context->pCodecCtx->width, context->pCodecCtx->height, context->pCodecCtx->pix_fmt,
                                     context->pCodecCtx->width, context->pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


    LOGI("[Input     ]%s\n", context->mp4_path);
    LOGI("[Format    ]%s\n", context->pFormatCtx->iformat->name);
    LOGI("[Codec     ]%s\n", context->pCodecCtx->codec->name);
    LOGI("[Resolution]%dx%d\n", context->pCodecCtx->width, context->pCodecCtx->height);

    LOGI("init gl show!\n");
    
    return context;
}

void decode_tag(MP4_DECODE_TAG * context)
{
    int scale_jpg_cnt = 0;
    int ret = -1;
    int y_size;
    int got_picture = -1;
    FILE *fp_yuv = NULL;
    unsigned char * frame_yuv = NULL;
    
    if(NULL == context)
    {
        return;
    }

    fp_yuv = fopen("/data/temp/test.yuv", "wb+");
    if(fp_yuv == NULL)
    {
        LOGE("Cannot open output file.\n");
        return;
    }

    context->time_start = clock();
    while(av_read_frame(context->pFormatCtx, context->packet) >= 0)
    {
        if(context->packet->stream_index == context->videoindex)
        {
            ret = avcodec_decode_video2(context->pCodecCtx, context->pFrame, &got_picture, context->packet);
            if(ret < 0)
            {
                LOGE("Decode Error.\n");
                return -1;
            }

            if(got_picture)
            {
                sws_scale(context->img_convert_ctx, (const uint8_t* const*)context->pFrame->data, context->pFrame->linesize, 0, context->pCodecCtx->height,
                          context->pFrameYUV->data, context->pFrameYUV->linesize);

                y_size = context->pCodecCtx->width * context->pCodecCtx->height;
                frame_yuv = (unsigned char *)malloc(y_size *3 / 2);
                memset(frame_yuv, 0, y_size *3 / 2);

                //snprintf(frame_yuv, y_size *3 / 2, "%s%s%s", context->pFrameYUV->data[0], context->pFrameYUV->data[1], context->pFrameYUV->data[2]);
                memcpy(frame_yuv, context->pFrameYUV->data[0], y_size);
                memcpy(frame_yuv + y_size, context->pFrameYUV->data[1], y_size / 4);
                memcpy(frame_yuv + y_size * 5 / 4, context->pFrameYUV->data[2], y_size / 4);

                //fwrite(frame_yuv, 1, y_size * 3 / 2, fp_yuv);

                if(++scale_jpg_cnt % 100 == 0)
                {
                    char jpg_name[32];
                    snprintf(jpg_name, 32 - 1, "scale_%d.jpeg", scale_jpg_cnt);
                    yuv_to_jpeg(jpg_name, frame_yuv, context->pCodecCtx->width, context->pCodecCtx->height);
                }
                
                //fwrite(context->pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
                //fwrite(context->pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
                //fwrite(context->pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
                //Output info
                char pictype_str[10] = {0};
                switch(context->pFrame->pict_type)
                {
                    case AV_PICTURE_TYPE_I:sprintf(pictype_str, "I"); break;
                    case AV_PICTURE_TYPE_P:sprintf(pictype_str, "P"); break;
                    case AV_PICTURE_TYPE_B:sprintf(pictype_str, "B"); break;
                    default:sprintf(pictype_str, "Other"); break;
                }

                FREE_MEM(frame_yuv);
                LOGI("Frame Index: %d. Type:%s\n", context->frame_cnt, pictype_str);
                context->frame_cnt++;
            }
        }
        
        av_free_packet(context->packet);
    }
    //flush decoder
    //FIX: Flush Frames remained in Codec
    while (1)
    {
        ret = avcodec_decode_video2(context->pCodecCtx, context->pFrame, &got_picture, context->packet);
        if (ret < 0)
            break;
        if (!got_picture)
            break;
        sws_scale(context->img_convert_ctx, (const uint8_t* const*)context->pFrame->data, context->pFrame->linesize, 0, context->pCodecCtx->height,
                  context->pFrameYUV->data, context->pFrameYUV->linesize);
        int y_size = context->pCodecCtx->width * context->pCodecCtx->height;
        fwrite(context->pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
        fwrite(context->pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
        fwrite(context->pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
        //Output info
        char pictype_str[10] = {0};
        switch(context->pFrame->pict_type)
        {
            case AV_PICTURE_TYPE_I:sprintf(pictype_str, "I"); break;
            case AV_PICTURE_TYPE_P:sprintf(pictype_str, "P"); break;
            case AV_PICTURE_TYPE_B:sprintf(pictype_str, "B"); break;
            default:sprintf(pictype_str, "Other"); break;
        }
        
        LOGI("Frame Index: %5d. Type:%s\n", context->frame_cnt, pictype_str);
        context->frame_cnt++;
    }

    if(NULL != fp_yuv)
    {
        fclose(fp_yuv);
        fp_yuv = NULL;
    }
}

void uninit_decode_tag(MP4_DECODE_TAG * context)
{
    double  time_duration = 0.0;
    
    if(NULL == context)
    {
        return;
    }

    context->time_finish = clock();
    time_duration = (double)(context->time_finish - context->time_start);

    LOGI("[Time      ]%fms\n", time_duration);
    LOGI("[Count     ]%d\n", context->frame_cnt);

    FREE_MEM(context->mp4_path);
    if(NULL != context->img_convert_ctx)
    {
        sws_freeContext(context->img_convert_ctx);
    }
    
    if(NULL != context->pFrameYUV)
    {
        av_frame_free(&(context->pFrameYUV));
    }

    if(NULL != context->pFrame)
    {
        av_frame_free(&(context->pFrame));
    }

    if(NULL != context->pCodecCtx)
    {
        avcodec_close(context->pCodecCtx);
    }

    if(NULL != context->pFormatCtx)
    {
        avformat_close_input(&(context->pFormatCtx));
    }

    FREE_MEM(context);
}

int yuv_to_jpeg(const char * out_file, const unsigned char * yuv_data, int width, int height)
{
    AVFormatContext* pFormatCtx = NULL;
    AVOutputFormat* fmt = NULL;
    AVStream* video_st = NULL;
    AVCodecContext* pCodecCtx = NULL;
    AVCodec* pCodec = NULL;
    uint8_t* picture_buf = NULL;
    AVFrame* picture = NULL;
    AVPacket pkt;
    int y_size;
    int got_picture = 0;
    int size;
    int ret = -1;
    int rv = -1;                                     //YUV source
    int in_w = width, in_h = height;                           //YUV's width and height
    //const char* out_file = "cuc_view_encode.jpg";             //Output file

    av_register_all();

    //Method 1
    pFormatCtx = avformat_alloc_context();
    //Guess format
    fmt = av_guess_format("mjpeg", NULL, NULL);
    pFormatCtx->oformat = fmt;
    //Output URL
    if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0)
    {
        printf("Couldn't open output file(%s), error: %d\n", out_file, strerror(errno));
        goto MAIN_EXIT;
    }

    //Method 2. More simple
    //avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
    //fmt = pFormatCtx->oformat;

    video_st = avformat_new_stream(pFormatCtx, 0);
    if (video_st == NULL)
    {
        goto MAIN_EXIT;
    }
    
    pCodecCtx = video_st->codec;
    pCodecCtx->codec_id = fmt->video_codec;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;

    pCodecCtx->width = in_w;  
    pCodecCtx->height = in_h;

    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = 25;   
    //Output some information
    av_dump_format(pFormatCtx, 0, out_file, 1);

    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec)
    {
        printf("Codec not found.");
        goto MAIN_EXIT;
    }
    
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("Could not open codec.");
        goto MAIN_EXIT;
    }
    
    picture = av_frame_alloc();
    size = avpicture_get_size(pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);
    picture_buf = (uint8_t *)av_malloc(size);
    if (!picture_buf)
    {
        goto MAIN_EXIT;
    }
    
    avpicture_fill((AVPicture *)picture, picture_buf, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height);

    //Write Header
    avformat_write_header(pFormatCtx, NULL);

    y_size = pCodecCtx->width * pCodecCtx->height;
    av_new_packet(&pkt, y_size * 3);
    //Read YUV

    memcpy(picture_buf, yuv_data, y_size * 3 / 2);
    
    //if (fread(picture_buf, 1, y_size * 3 / 2, in_file) <= 0)
    //{
        //printf("Could not read input file.");
        //av_free_packet(&pkt);
        //goto MAIN_EXIT;
    //}
    
    picture->data[0] = picture_buf;              // Y
    picture->data[1] = picture_buf+ y_size;      // U 
    picture->data[2] = picture_buf+ y_size*5/4;  // V

    //Encode
    ret = avcodec_encode_video2(pCodecCtx, &pkt,picture, &got_picture);
    if(ret < 0)
    {
        printf("Encode Error.\n");
        av_free_packet(&pkt);
        goto MAIN_EXIT;
    }
    
    if (got_picture == 1)
    {
        pkt.stream_index = video_st->index;
        ret = av_write_frame(pFormatCtx, &pkt);
    }

    av_free_packet(&pkt);
    //Write Trailer
    av_write_trailer(pFormatCtx);

    printf("Encode Successful.\n");
    rv  = 0;
    
MAIN_EXIT:
    if (NULL != video_st)
    {
        avcodec_close(video_st->codec);
        if(NULL != picture)
        {

            av_free(picture);
            picture = NULL;
        }

        if(NULL != picture_buf)
        {
            av_free(picture_buf);
            picture_buf = NULL;
        }
    }

    if(NULL != pCodecCtx)
    {
        avcodec_close(pCodecCtx);
    }

    if(NULL != pFormatCtx)
    {
        avio_close(pFormatCtx->pb);
        avformat_free_context(pFormatCtx);
        pFormatCtx = NULL;
    }
    
    return rv ;
}

int decode(char * input_str, char * output_str)
{
    AVFormatContext *pFormatCtx;
    int             i, videoindex;
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;
    AVFrame *pFrame,*pFrameYUV;
    uint8_t *out_buffer;
    AVPacket *packet;
    int y_size;
    int ret, got_picture;
    struct SwsContext *img_convert_ctx;
    FILE *fp_yuv = NULL;
    int frame_cnt;
    clock_t time_start, time_finish;
    double  time_duration = 0.0;

    if(input_str == NULL || output_str == NULL)
    {
        return -1;
    }

    //FFmpeg av_log() callback
    av_log_set_callback(NULL);

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx, input_str, NULL, NULL) != 0)
    {
        LOGE("Couldn't open input stream.\n");
        return -1;
    }
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        LOGE("Couldn't find stream information.\n");
        return -1;
    }
    videoindex = -1;
    for(i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoindex = i;
            break;
        }
    }
    
    if(videoindex==-1)
    {
        LOGE("Couldn't find a video stream.\n");
        return -1;
    }
    
    pCodecCtx = pFormatCtx->streams[videoindex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec == NULL)
    {
        LOGE("Couldn't find Codec.\n");
        return -1;
    }
    
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        LOGE("Couldn't open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


    LOGI("[Input     ]%s\n", input_str);
    LOGI("[Output    ]%s\n", output_str);
    LOGI("[Format    ]%s\n", pFormatCtx->iformat->name);
    LOGI("[Codec     ]%s\n", pCodecCtx->codec->name);
    LOGI("[Resolution]%dx%d\n", pCodecCtx->width, pCodecCtx->height);


    fp_yuv = fopen(output_str, "wb+");
    if(fp_yuv == NULL)
    {
        LOGE("Cannot open output file.\n");
        return -1;
    }
    
    frame_cnt = 0;
    time_start = clock();

    while(av_read_frame(pFormatCtx, packet) >= 0)
    {
        if(packet->stream_index == videoindex)
        {
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if(ret < 0)
            {
                LOGE("Decode Error.\n");
                return -1;
            }

            if(got_picture)
            {
                sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                          pFrameYUV->data, pFrameYUV->linesize);

                y_size = pCodecCtx->width*pCodecCtx->height;
                fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
                fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
                fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
                //Output info
                char pictype_str[10] = {0};
                switch(pFrame->pict_type)
                {
                    case AV_PICTURE_TYPE_I:sprintf(pictype_str, "I"); break;
                    case AV_PICTURE_TYPE_P:sprintf(pictype_str, "P"); break;
                    case AV_PICTURE_TYPE_B:sprintf(pictype_str, "B"); break;
                    default:sprintf(pictype_str, "Other"); break;
                }

                LOGI("Frame Index: %d. Type:%s\n", frame_cnt, pictype_str);
                frame_cnt++;
            }
        }
        
        av_free_packet(packet);
    }
    //flush decoder
    //FIX: Flush Frames remained in Codec
    while (1) 
    {
        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
        if (ret < 0)
            break;
        if (!got_picture)
            break;
        sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                  pFrameYUV->data, pFrameYUV->linesize);
        int y_size = pCodecCtx->width * pCodecCtx->height;
        fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
        fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
        fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
        //Output info
        char pictype_str[10] = {0};
        switch(pFrame->pict_type)
        {
            case AV_PICTURE_TYPE_I:sprintf(pictype_str, "I"); break;
            case AV_PICTURE_TYPE_P:sprintf(pictype_str, "P"); break;
            case AV_PICTURE_TYPE_B:sprintf(pictype_str, "B"); break;
            default:sprintf(pictype_str, "Other"); break;
        }
        
        LOGI("Frame Index: %5d. Type:%s\n", frame_cnt, pictype_str);
        frame_cnt++;
    }
    
    time_finish = clock();
    time_duration = (double)(time_finish - time_start);

    LOGI("[Time      ]%fms\n", time_duration);
    LOGI("[Count     ]%d\n", frame_cnt);

    sws_freeContext(img_convert_ctx);

    fclose(fp_yuv);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
