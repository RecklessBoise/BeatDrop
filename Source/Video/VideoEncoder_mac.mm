#include "VideoEncoder.h"
#include "Core/Text.h"

#include <juce_audio_formats/juce_audio_formats.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

namespace beatdrop
{

namespace
{
    NSString* toNS (const juce::String& s)
    {
        return [NSString stringWithUTF8String: s.toRawUTF8()];
    }

    juce::String fromNS (NSString* s)
    {
        return s != nil ? juce::String::fromUTF8 ([s UTF8String]) : juce::String();
    }

    CVPixelBufferRef createPixelBuffer (const juce::Image& source)
    {
        const auto image = source.convertedToFormat (juce::Image::ARGB);
        const auto width = image.getWidth();
        const auto height = image.getHeight();

        NSDictionary* attributes = @{
            (id) kCVPixelBufferCGImageCompatibilityKey: @YES,
            (id) kCVPixelBufferCGBitmapContextCompatibilityKey: @YES,
            (id) kCVPixelBufferIOSurfacePropertiesKey: @{}
        };

        CVPixelBufferRef buffer = nullptr;

        if (CVPixelBufferCreate (kCFAllocatorDefault, (size_t) width, (size_t) height, kCVPixelFormatType_32BGRA,
                                 (CFDictionaryRef) attributes, &buffer) != kCVReturnSuccess)
            return nullptr;

        CVPixelBufferLockBaseAddress (buffer, 0);

        auto* base = static_cast<uint8_t*> (CVPixelBufferGetBaseAddress (buffer));
        const auto bytesPerRow = CVPixelBufferGetBytesPerRow (buffer);
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < height; ++y)
        {
            auto* row = base + (size_t) y * bytesPerRow;

            for (int x = 0; x < width; ++x)
            {
                const auto colour = pixels.getPixelColour (x, y);
                row[x * 4 + 0] = colour.getBlue();
                row[x * 4 + 1] = colour.getGreen();
                row[x * 4 + 2] = colour.getRed();
                row[x * 4 + 3] = 255;
            }
        }

        CVPixelBufferUnlockBaseAddress (buffer, 0);
        return buffer;
    }

    NSData* makeChannelLayout (int numChannels)
    {
        AudioChannelLayout layout {};
        layout.mChannelLayoutTag = numChannels == 1 ? kAudioChannelLayoutTag_Mono : kAudioChannelLayoutTag_Stereo;
        return [NSData dataWithBytes: &layout length: sizeof (layout)];
    }
}

bool isVideoEncodingSupported()
{
    return true;
}

juce::Result encodeStillImageVideo (const VideoEncodeRequest& request, const std::function<bool (double)>& progress)
{
    if (! request.frame.isValid())
        return juce::Result::fail ("Aucune image pour la vidéo."_t);

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> audioReader (formatManager.createReaderFor (request.audioFile));

    if (audioReader == nullptr || audioReader->lengthInSamples <= 0 || audioReader->sampleRate <= 0)
        return juce::Result::fail ("Impossible de lire l'audio : " + request.audioFile.getFileName());

    const auto durationSeconds = (double) audioReader->lengthInSamples / audioReader->sampleRate;
    const auto numChannels = juce::jlimit (1, 2, (int) audioReader->numChannels);
    const auto outputSampleRate = audioReader->sampleRate <= 48000.0 ? audioReader->sampleRate : 48000.0;
    audioReader.reset();

    const auto fps = juce::jmax (1, request.framesPerSecond);
    const auto totalFrames = juce::jmax ((juce::int64) 1, (juce::int64) std::ceil (durationSeconds * fps));

    request.outputFile.getParentDirectory().createDirectory();
    request.outputFile.deleteFile();

    @autoreleasepool
    {
        NSError* error = nil;

        AVAssetWriter* writer = [AVAssetWriter assetWriterWithURL: [NSURL fileURLWithPath: toNS (request.outputFile.getFullPathName())]
                                                         fileType: AVFileTypeMPEG4
                                                            error: &error];
        if (writer == nil)
            return juce::Result::fail ("AVAssetWriter : " + fromNS (error.localizedDescription));

        writer.shouldOptimizeForNetworkUse = YES;

        // --- video input
        const auto width = request.frame.getWidth();
        const auto height = request.frame.getHeight();

        NSDictionary* videoSettings = @{
            AVVideoCodecKey: AVVideoCodecTypeH264,
            AVVideoWidthKey: @(width),
            AVVideoHeightKey: @(height),
            AVVideoCompressionPropertiesKey: @{
                AVVideoAverageBitRateKey: @(2000000),
                AVVideoMaxKeyFrameIntervalKey: @(fps * 2),
                AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel
            }
        };

        AVAssetWriterInput* videoInput = [AVAssetWriterInput assetWriterInputWithMediaType: AVMediaTypeVideo
                                                                            outputSettings: videoSettings];
        videoInput.expectsMediaDataInRealTime = NO;

        AVAssetWriterInputPixelBufferAdaptor* adaptor =
            [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput: videoInput
                                                                             sourcePixelBufferAttributes: @{
                (id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                (id) kCVPixelBufferWidthKey: @(width),
                (id) kCVPixelBufferHeightKey: @(height)
            }];

        // --- audio input (AAC)
        NSData* channelLayout = makeChannelLayout (numChannels);

        NSDictionary* audioSettings = @{
            AVFormatIDKey: @(kAudioFormatMPEG4AAC),
            AVSampleRateKey: @(outputSampleRate),
            AVNumberOfChannelsKey: @(numChannels),
            AVEncoderBitRateKey: @(numChannels == 1 ? 128000 : 256000),
            AVChannelLayoutKey: channelLayout
        };

        AVAssetWriterInput* audioInput = [AVAssetWriterInput assetWriterInputWithMediaType: AVMediaTypeAudio
                                                                            outputSettings: audioSettings];
        audioInput.expectsMediaDataInRealTime = NO;

        if (! [writer canAddInput: videoInput] || ! [writer canAddInput: audioInput])
            return juce::Result::fail ("Configuration vidéo refusée par AVFoundation."_t);

        [writer addInput: videoInput];
        [writer addInput: audioInput];

        // --- audio source, decoded to float PCM
        AVURLAsset* asset = [AVURLAsset URLAssetWithURL: [NSURL fileURLWithPath: toNS (request.audioFile.getFullPathName())]
                                                options: nil];

        JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
        NSArray<AVAssetTrack*>* tracks = [asset tracksWithMediaType: AVMediaTypeAudio];
        JUCE_END_IGNORE_DEPRECATION_WARNINGS

        if (tracks.count == 0)
            return juce::Result::fail ("Le fichier audio ne contient pas de piste lisible par AVFoundation.");

        AVAssetReader* assetReader = [AVAssetReader assetReaderWithAsset: asset error: &error];

        if (assetReader == nil)
            return juce::Result::fail ("AVAssetReader : " + fromNS (error.localizedDescription));

        NSDictionary* pcmSettings = @{
            AVFormatIDKey: @(kAudioFormatLinearPCM),
            AVSampleRateKey: @(outputSampleRate),
            AVNumberOfChannelsKey: @(numChannels),
            AVChannelLayoutKey: channelLayout,
            AVLinearPCMBitDepthKey: @32,
            AVLinearPCMIsFloatKey: @YES,
            AVLinearPCMIsBigEndianKey: @NO,
            AVLinearPCMIsNonInterleaved: @NO
        };

        AVAssetReaderTrackOutput* audioOutput = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack: tracks[0]
                                                                                           outputSettings: pcmSettings];
        if (! [assetReader canAddOutput: audioOutput])
            return juce::Result::fail ("Conversion audio impossible.");

        [assetReader addOutput: audioOutput];

        if (! [assetReader startReading])
            return juce::Result::fail ("Lecture audio impossible : " + fromNS (assetReader.error.localizedDescription));

        // --- write
        CVPixelBufferRef pixelBuffer = createPixelBuffer (request.frame);

        if (pixelBuffer == nullptr)
            return juce::Result::fail ("Impossible de préparer l'image de la vidéo."_t);

        if (! [writer startWriting])
        {
            CVPixelBufferRelease (pixelBuffer);
            return juce::Result::fail ("Écriture vidéo impossible : "_t + fromNS (writer.error.localizedDescription));
        }

        [writer startSessionAtSourceTime: kCMTimeZero];

        juce::int64 framesWritten = 0;
        double audioSecondsWritten = 0.0;
        bool videoDone = false, audioDone = false;
        juce::String failure;

        while (failure.isEmpty() && ! (videoDone && audioDone))
        {
            @autoreleasepool
            {
                bool didWork = false;

                if (! videoDone && videoInput.readyForMoreMediaData)
                {
                    if (framesWritten < totalFrames)
                    {
                        if ([adaptor appendPixelBuffer: pixelBuffer withPresentationTime: CMTimeMake (framesWritten, (int32_t) fps)])
                            ++framesWritten;
                        else
                            failure = "Encodage vidéo : "_t + fromNS (writer.error.localizedDescription);
                    }
                    else
                    {
                        [videoInput markAsFinished];
                        videoDone = true;
                    }

                    didWork = true;
                }

                if (failure.isEmpty() && ! audioDone && audioInput.readyForMoreMediaData)
                {
                    if (CMSampleBufferRef sample = [audioOutput copyNextSampleBuffer])
                    {
                        const auto end = CMTimeAdd (CMSampleBufferGetPresentationTimeStamp (sample), CMSampleBufferGetDuration (sample));
                        audioSecondsWritten = CMTimeGetSeconds (end);

                        if (! [audioInput appendSampleBuffer: sample])
                            failure = "Encodage audio : " + fromNS (writer.error.localizedDescription);

                        CFRelease (sample);
                    }
                    else if (assetReader.status == AVAssetReaderStatusFailed)
                    {
                        failure = "Lecture audio : " + fromNS (assetReader.error.localizedDescription);
                    }
                    else
                    {
                        [audioInput markAsFinished];
                        audioDone = true;
                    }

                    didWork = true;
                }

                if (failure.isEmpty() && writer.status == AVAssetWriterStatusFailed)
                    failure = "Encodage : " + fromNS (writer.error.localizedDescription);

                const auto fraction = 0.5 * (double) framesWritten / (double) totalFrames
                                    + 0.5 * juce::jlimit (0.0, 1.0, audioSecondsWritten / durationSeconds);

                if (failure.isEmpty() && progress != nullptr && ! progress (fraction))
                    failure = "Rendu vidéo annulé."_t;

                if (! didWork)
                    juce::Thread::sleep (2);
            }
        }

        CVPixelBufferRelease (pixelBuffer);

        if (failure.isNotEmpty())
        {
            [assetReader cancelReading];
            [writer cancelWriting];
            request.outputFile.deleteFile();
            return juce::Result::fail (failure);
        }

        [writer endSessionAtSourceTime: CMTimeMakeWithSeconds (durationSeconds, 48000)];

        juce::WaitableEvent finished;
        auto* finishedPtr = &finished;
        [writer finishWritingWithCompletionHandler: ^{ finishedPtr->signal(); }];

        if (! finished.wait (120000) || writer.status != AVAssetWriterStatusCompleted)
        {
            request.outputFile.deleteFile();
            return juce::Result::fail ("Finalisation du MP4 impossible : " + fromNS (writer.error.localizedDescription));
        }
    }

    if (progress != nullptr)
        progress (1.0);

    return juce::Result::ok();
}

} // namespace beatdrop
