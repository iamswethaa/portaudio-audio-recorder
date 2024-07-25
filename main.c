#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define NUM_CHANNELS 2
#define SAMPLE_FORMAT paInt16
#define RECORD_DURATION 5 // seconds

typedef struct {
    int frameIndex;
    int maxFrameIndex;
    short *recordedSamples;
} paTestData;

static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData) {
    paTestData *data = (paTestData*)userData;
    const short *rptr = (const short*)inputBuffer;
    short *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    unsigned long framesToCalc;
    unsigned long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) outputBuffer;

    if (framesLeft < framesPerBuffer) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL) {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = 0;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = 0;  /* right */
        }
    } else {
        for (i = 0; i < framesToCalc; i++) {
            *wptr++ = *rptr++;  /* left */
            if (NUM_CHANNELS == 2) *wptr++ = *rptr++;  /* right */
        }
    }
    data->frameIndex += framesToCalc;
    return finished;
}

static int playCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData) {
    paTestData *data = (paTestData*)userData;
    short *rptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    short *wptr = (short*)outputBuffer;
    unsigned long framesToCalc;
    unsigned long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) inputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;

    if (framesLeft < framesPerBuffer) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    for (i = 0; i < framesToCalc; i++) {
        *wptr++ = *rptr++;  /* left */
        if (NUM_CHANNELS == 2) *wptr++ = *rptr++;  /* right */
    }
    data->frameIndex += framesToCalc;
    return finished;
}

int main(void) {
    PaError err;
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    paTestData data;
    int totalFrames;
    int numSamples;
    int numBytes;

    data.maxFrameIndex = totalFrames = SAMPLE_RATE * RECORD_DURATION;
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(short);
    data.recordedSamples = (short *) malloc(numBytes);
    if (data.recordedSamples == NULL) {
        printf("Could not allocate record array.\n");
        return 1;
    }
    for (int i = 0; i < numSamples; i++) data.recordedSamples[i] = 0;

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        printf("Error: No default input device.\n");
        goto error;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = SAMPLE_FORMAT;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
            &stream,
            &inputParameters,
            NULL,  /* no output */
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,
            recordCallback,
            &data);
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    printf("Recording for %d seconds...\n", RECORD_DURATION);
    while ((err = Pa_IsStreamActive(stream)) == 1) Pa_Sleep(100);
    if (err < 0) goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error;

    printf("Recording complete. Now playing back...\n");

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        printf("Error: No default output device.\n");
        goto error;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = SAMPLE_FORMAT;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    data.frameIndex = 0;

    err = Pa_OpenStream(
            &stream,
            NULL, /* no input */
            &outputParameters,
            SAMPLE_RATE,
            FRAMES_PER_BUFFER,
            paClipOff,
            playCallback,
            &data);
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    while ((err = Pa_IsStreamActive(stream)) == 1) Pa_Sleep(100);
    if (err < 0) goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error;

    Pa_Terminate();
    free(data.recordedSamples);
    printf("Playback complete.\n");
    return 0;

error:
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the PortAudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    free(data.recordedSamples);
    return err;
}
