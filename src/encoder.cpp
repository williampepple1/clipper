#include "encoder.h"
#include <QDebug>
#include <QThread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

Encoder::Encoder(QObject *parent)
    : QObject(parent)
{
}

Encoder::~Encoder()
{
    stopEncoding();
    finalize();
}

bool Encoder::initialize(const EncodeParams &params)
{
    if (m_initialized) return true;
    m_params = params;

    AVFormatContext *fmtCtx = nullptr;
    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr, "mp4",
                                             m_params.outputPath.toUtf8().constData());
    if (ret < 0 || !fmtCtx) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        emit encodingError(QString("Failed to create output context: %1").arg(errBuf));
        return false;
    }
    m_fmtCtx = fmtCtx;
    m_initialized = true;
    return true;
}

void Encoder::encodeLoop()
{
    m_encoding = true;
    auto *fmtCtx = static_cast<AVFormatContext *>(m_fmtCtx);

    if (!initVideoEncoder() || !initAudioEncoder()) {
        m_encoding = false;
        finalize();
        emit encodingStopped();
        return;
    }

    int ret = avio_open(&fmtCtx->pb, m_params.outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        emit encodingError(QString("Failed to open output file: %1").arg(errBuf));
        m_encoding = false;
        finalize();
        emit encodingStopped();
        return;
    }

    ret = avformat_write_header(fmtCtx, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        emit encodingError(QString("Failed to write header: %1").arg(errBuf));
        m_encoding = false;
        finalize();
        emit encodingStopped();
        return;
    }

    m_streamsInitialized = true;
    emit encodingStarted();

    auto *vCtx = static_cast<AVCodecContext *>(m_videoCodecCtx);
    auto *aCtx = static_cast<AVCodecContext *>(m_audioCodecCtx);
    auto *vStream = static_cast<AVStream *>(m_videoStream);
    auto *aStream = static_cast<AVStream *>(m_audioStream);
    auto *vFrame = static_cast<AVFrame *>(m_videoFrame);
    auto *aFrame = static_cast<AVFrame *>(m_audioFrame);

    AVPacket *pkt = av_packet_alloc();
    double startTimeSec = -1.0;
    m_audioFrameSize = m_params.channels * (sizeof(int16_t));

    while (m_encoding.load(std::memory_order_acquire)) {
        bool didWork = false;

        FramePacket vPkt;
        {
            QMutexLocker lock(&m_videoMutex);
            if (!m_videoQueue.isEmpty()) {
                vPkt = m_videoQueue.dequeue();
                didWork = true;
            }
        }

        if (didWork && !vPkt.image.isNull()) {
            if (startTimeSec < 0) startTimeSec = vPkt.timestampUs / 1000000.0;

            QImage rgb = (vPkt.image.format() == QImage::Format_RGB32)
                ? vPkt.image
                : vPkt.image.convertToFormat(QImage::Format_RGB32);
            uint8_t *srcData[1] = {rgb.bits()};
            int srcStride[1] = {static_cast<int>(rgb.bytesPerLine())};

            av_frame_make_writable(vFrame);
            sws_scale(static_cast<SwsContext *>(m_swsCtx),
                      srcData, srcStride, 0, m_params.height,
                      vFrame->data, vFrame->linesize);

            double ptsSec = (vPkt.timestampUs / 1000000.0) - startTimeSec;
            vFrame->pts = static_cast<int64_t>(ptsSec / av_q2d(vCtx->time_base));

            ret = avcodec_send_frame(vCtx, vFrame);
            while (ret >= 0) {
                ret = avcodec_receive_packet(vCtx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) break;
                av_packet_rescale_ts(pkt, vCtx->time_base, vStream->time_base);
                pkt->stream_index = vStream->index;
                av_interleaved_write_frame(fmtCtx, pkt);
                av_packet_unref(pkt);
            }

            m_frameCount++;
            double elapsed = (vPkt.timestampUs / 1000000.0) - startTimeSec;
            emit progressUpdated(m_frameCount, elapsed);
        }

        AudioPacket aPkt;
        bool hasAudio = false;
        {
            QMutexLocker lock(&m_audioMutex);
            if (!m_audioQueue.isEmpty()) {
                aPkt = m_audioQueue.dequeue();
                hasAudio = true;
            }
        }

        if (hasAudio && !aPkt.data.isEmpty()) {
            if (startTimeSec < 0) startTimeSec = aPkt.timestampUs / 1000000.0;

            QByteArray combined = m_audioRemainder + aPkt.data;
            const uint8_t *src = reinterpret_cast<const uint8_t *>(combined.constData());
            int srcSamples = combined.size() / m_audioFrameSize;
            const uint8_t *srcPtr = src;

            av_frame_make_writable(aFrame);
            ret = swr_convert(static_cast<SwrContext *>(m_swrCtx),
                              aFrame->data, aFrame->nb_samples,
                              &srcPtr, srcSamples);
            if (ret > 0) {
                aFrame->nb_samples = ret;
                aFrame->pts = m_audioPts;
                m_audioPts += ret;

                ret = avcodec_send_frame(aCtx, aFrame);
                while (ret >= 0) {
                    ret = avcodec_receive_packet(aCtx, pkt);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                    if (ret < 0) break;
                    av_packet_rescale_ts(pkt, aCtx->time_base, aStream->time_base);
                    pkt->stream_index = aStream->index;
                    av_interleaved_write_frame(fmtCtx, pkt);
                    av_packet_unref(pkt);
                }
            }

            int consumed = static_cast<int>(srcPtr - src);
            if (consumed < combined.size() && combined.size() < 65536)
                m_audioRemainder = combined.mid(consumed);
            else
                m_audioRemainder.clear();
        }

        if (!didWork && !hasAudio) {
            QMutexLocker vLock(&m_videoMutex);
            if (m_videoQueue.isEmpty()) {
                m_videoCond.wait(&m_videoMutex, 10);
            }
        }
    }

    // Flush
    avcodec_send_frame(vCtx, nullptr);
    while (avcodec_receive_packet(vCtx, pkt) == 0) {
        av_packet_rescale_ts(pkt, vCtx->time_base, vStream->time_base);
        pkt->stream_index = vStream->index;
        av_interleaved_write_frame(fmtCtx, pkt);
        av_packet_unref(pkt);
    }

    avcodec_send_frame(aCtx, nullptr);
    while (avcodec_receive_packet(aCtx, pkt) == 0) {
        av_packet_rescale_ts(pkt, aCtx->time_base, aStream->time_base);
        pkt->stream_index = aStream->index;
        av_interleaved_write_frame(fmtCtx, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(fmtCtx);
    av_packet_free(&pkt);

    {
        QMutexLocker vLock(&m_videoMutex);
        m_videoQueue.clear();
        QMutexLocker aLock(&m_audioMutex);
        m_audioQueue.clear();
    }
    m_audioRemainder.clear();

    finalize();

    emit encodingStopped();
}

void Encoder::stopEncoding()
{
    m_encoding = false;
    QMutexLocker vLock(&m_videoMutex);
    m_videoCond.wakeAll();
}

void Encoder::pushVideoFrame(const QImage &image, qint64 timestampUs)
{
    QMutexLocker lock(&m_videoMutex);
    if (m_videoQueue.size() < MAX_QUEUE_SIZE) {
        m_videoQueue.enqueue({image, timestampUs});
        m_videoCond.wakeOne();
    }
}

void Encoder::pushAudioData(const QByteArray &data, qint64 timestampUs)
{
    QMutexLocker lock(&m_audioMutex);
    if (m_audioQueue.size() < MAX_QUEUE_SIZE) {
        m_audioQueue.enqueue({data, timestampUs});
        m_audioCond.wakeOne();
    }
}

bool Encoder::initVideoEncoder()
{
    auto *fmtCtx = static_cast<AVFormatContext *>(m_fmtCtx);

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        emit encodingError("H.264 encoder not found. Install FFmpeg with libx264.");
        return false;
    }

    AVStream *stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) { emit encodingError("Failed to create video stream"); return false; }
    m_videoStreamIdx = stream->index;
    m_videoStream = stream;

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) { emit encodingError("Failed to alloc video codec context"); return false; }
    m_videoCodecCtx = codecCtx;

    codecCtx->width = m_params.width;
    codecCtx->height = m_params.height;
    codecCtx->time_base = AVRational{1, m_params.fps};
    codecCtx->framerate = AVRational{m_params.fps, 1};
    codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx->bit_rate = m_params.videoBitrate;
    codecCtx->gop_size = m_params.fps * 2;
    codecCtx->max_b_frames = 2;

    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_opt_set(codecCtx->priv_data, "preset", "fast", 0);
    av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0);

    int ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        emit encodingError(QString("Failed to open video codec: %1").arg(errBuf));
        return false;
    }

    ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) { emit encodingError("Failed to copy video codec params"); return false; }

    m_swsCtx = sws_getContext(m_params.width, m_params.height, AV_PIX_FMT_RGB32,
                              m_params.width, m_params.height, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) { emit encodingError("Failed to create swscale context"); return false; }

    AVFrame *frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = m_params.width;
    frame->height = m_params.height;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) { emit encodingError("Failed to alloc video frame buffer"); return false; }
    m_videoFrame = frame;

    return true;
}

bool Encoder::initAudioEncoder()
{
    auto *fmtCtx = static_cast<AVFormatContext *>(m_fmtCtx);

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        emit encodingError("AAC encoder not found");
        return false;
    }

    AVStream *stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) { emit encodingError("Failed to create audio stream"); return false; }
    m_audioStreamIdx = stream->index;
    m_audioStream = stream;

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) { emit encodingError("Failed to alloc audio codec context"); return false; }
    m_audioCodecCtx = codecCtx;

    codecCtx->sample_rate = m_params.sampleRate;
    codecCtx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    codecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codecCtx->bit_rate = 192000;
    codecCtx->time_base = AVRational{1, m_params.sampleRate};

    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        emit encodingError(QString("Failed to open audio codec: %1").arg(errBuf));
        return false;
    }

    ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) { emit encodingError("Failed to copy audio codec params"); return false; }

    AVChannelLayout inLayout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    int ret2 = swr_alloc_set_opts2(reinterpret_cast<SwrContext **>(&m_swrCtx),
        &outLayout, AV_SAMPLE_FMT_FLTP, m_params.sampleRate,
        &inLayout, AV_SAMPLE_FMT_S16, m_params.sampleRate, 0, nullptr);
    if (ret2 < 0 || !m_swrCtx) { emit encodingError("Failed to create resampler"); return false; }
    swr_init(static_cast<SwrContext *>(m_swrCtx));

    AVFrame *frame = av_frame_alloc();
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = m_params.sampleRate;
    frame->ch_layout = outLayout;
    frame->nb_samples = 1024;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) { emit encodingError("Failed to alloc audio frame buffer"); return false; }
    m_audioFrame = frame;

    return true;
}

void Encoder::finalize()
{
    bool expected = false;
    if (!m_finalized.compare_exchange_strong(expected, true))
        return;

    auto *fmtCtx = static_cast<AVFormatContext *>(m_fmtCtx);
    if (fmtCtx) {
        if (fmtCtx->pb) avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        m_fmtCtx = nullptr;
    }
    if (m_videoCodecCtx) {
        avcodec_free_context(reinterpret_cast<AVCodecContext **>(&m_videoCodecCtx));
    }
    if (m_audioCodecCtx) {
        avcodec_free_context(reinterpret_cast<AVCodecContext **>(&m_audioCodecCtx));
    }
    if (m_videoFrame) av_frame_free(reinterpret_cast<AVFrame **>(&m_videoFrame));
    if (m_audioFrame) av_frame_free(reinterpret_cast<AVFrame **>(&m_audioFrame));
    if (m_swsCtx) sws_freeContext(static_cast<SwsContext *>(m_swsCtx));
    if (m_swrCtx) swr_free(reinterpret_cast<SwrContext **>(&m_swrCtx));
}
