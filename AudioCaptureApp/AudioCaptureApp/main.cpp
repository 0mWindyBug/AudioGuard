#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <fstream>
#include <cstdint>

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Ole32.lib")

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

// Fixed macro definitions with proper bracing
#define EXIT_ON_ERROR(hres) \
    if (FAILED(hres)) { goto Exit; }

#define SAFE_RELEASE(punk) \
    if ((punk) != NULL) { \
        (punk)->Release(); \
        (punk) = NULL; \
    }

struct WaveHeader {
    char riff[4] = { 'R', 'I', 'F', 'F' };
    uint32_t chunkSize = 0;
    char wave[4] = { 'W', 'A', 'V', 'E' };
    char fmt[4] = { 'f', 'm', 't', ' ' };
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint32_t byteRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    char data[4] = { 'd', 'a', 't', 'a' };
    uint32_t dataSize = 0;
};

int main() {
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsActualDuration;
    UINT32 bufferFrameCount;
    UINT32 numFramesAvailable;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioClient* pAudioClient = NULL;
    IAudioCaptureClient* pCaptureClient = NULL;
    WAVEFORMATEX* pwfx = NULL;
    UINT32 packetLength = 0;
    BOOL bDone = FALSE;
    BYTE* pData;
    DWORD flags;
    std::ofstream outFile("audio.wav", std::ios::binary);

    if (!outFile.is_open()) {
        return 1;  // Error handling for file opening
    }

    WaveHeader waveHeader;

    hr = CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY);
    EXIT_ON_ERROR(hr)

        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator), NULL,
            CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
            (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

        hr = pEnumerator->GetDefaultAudioEndpoint(
            eCapture, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

        hr = pDevice->Activate(
            __uuidof(IAudioClient), CLSCTX_ALL,
            NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr)

        // Adjust wave header with audio format
        waveHeader.numChannels = pwfx->nChannels;
    waveHeader.sampleRate = pwfx->nSamplesPerSec;
    waveHeader.byteRate = pwfx->nAvgBytesPerSec;
    waveHeader.blockAlign = pwfx->nBlockAlign;
    waveHeader.bitsPerSample = pwfx->wBitsPerSample;


    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        hnsRequestedDuration,
        0,
        pwfx,
        NULL);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->GetService(
            __uuidof(IAudioCaptureClient),
            (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr)

        hr = pAudioClient->Start();
    EXIT_ON_ERROR(hr)

        // Write wave header to output file
        outFile.write(reinterpret_cast<char*>(&waveHeader), sizeof(waveHeader));

    // Record for 1 minute (60 seconds)
    for (int i = 0; i < 60; i++) {
        Sleep(1000); // Wait for 1 second

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)

            while (packetLength != 0) {
                hr = pCaptureClient->GetBuffer(
                    &pData,
                    &numFramesAvailable,
                    &flags, NULL, NULL);
                EXIT_ON_ERROR(hr)

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        pData = NULL;  // Tell CopyData to write silence.
                    }

                // Only write if pData is not NULL
                if (pData != NULL) {
                    outFile.write(reinterpret_cast<char*>(pData),
                        numFramesAvailable * pwfx->nBlockAlign);
                    waveHeader.dataSize += numFramesAvailable * pwfx->nBlockAlign;
                }

                hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
                EXIT_ON_ERROR(hr)

                    hr = pCaptureClient->GetNextPacketSize(&packetLength);
                EXIT_ON_ERROR(hr)
            }
    }

    hr = pAudioClient->Stop();
    EXIT_ON_ERROR(hr)

        Exit:
    // Update chunk size in wave header
    waveHeader.chunkSize = waveHeader.dataSize + 36;

    // Rewrite wave header to output file with updated chunk and data size
    outFile.seekp(0, std::ios::beg);
    outFile.write(reinterpret_cast<char*>(&waveHeader), sizeof(waveHeader));

    outFile.close();
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
        SAFE_RELEASE(pDevice)
        SAFE_RELEASE(pAudioClient)
        SAFE_RELEASE(pCaptureClient)
        CoUninitialize();

    return hr;
}