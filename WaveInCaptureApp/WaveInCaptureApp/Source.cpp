#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>

#pragma comment(lib, "winmm.lib")

class AudioRecorder {
private:
    struct WAVHeader {
        char riffId[4] = { 'R', 'I', 'F', 'F' };
        uint32_t riffSize = 0;
        char waveId[4] = { 'W', 'A', 'V', 'E' };
        char fmtId[4] = { 'f', 'm', 't', ' ' };
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 44100;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char dataId[4] = { 'd', 'a', 't', 'a' };
        uint32_t dataSize = 0;
    };

    HWAVEIN hWaveIn = nullptr;
    std::vector<WAVEHDR> waveHeaders;
    std::vector<std::vector<BYTE>> audioBuffers;
    std::vector<BYTE> recordedData;
    bool isRecording = false;
    WAVHeader wavHeader;
    std::string outputFilename;
    std::string lastError;
    bool dataReceived = false;

    static constexpr int BUFFER_COUNT = 3;
    static constexpr int BUFFER_SIZE = 8192;

    static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
        DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
        if (uMsg == WIM_DATA) {
            auto* recorder = reinterpret_cast<AudioRecorder*>(dwInstance);
            auto* pHdr = reinterpret_cast<WAVEHDR*>(dwParam1);

            if (recorder->isRecording && pHdr->dwBytesRecorded > 0) {
                recorder->dataReceived = true;  // Set flag indicating data was received
                recorder->recordedData.insert(
                    recorder->recordedData.end(),
                    pHdr->lpData,
                    pHdr->lpData + pHdr->dwBytesRecorded
                );

                // Only add buffer back if we're still recording
                if (recorder->isRecording) {
                    waveInAddBuffer(hwi, pHdr, sizeof(WAVEHDR));
                }
            }
        }
    }

public:
    explicit AudioRecorder(const char* filename = "output.wav")
        : outputFilename(filename) {
        audioBuffers.resize(BUFFER_COUNT);
        waveHeaders.resize(BUFFER_COUNT);

        for (auto& buffer : audioBuffers) {
            buffer.resize(BUFFER_SIZE);
        }
    }

    ~AudioRecorder() {
        if (isRecording) {
            stopRecording();
        }
    }

    static std::vector<std::string> listInputDevices() {
        std::vector<std::string> devices;
        UINT numDevices = waveInGetNumDevs();

        for (UINT i = 0; i < numDevices; i++) {
            WAVEINCAPSA caps;
            if (waveInGetDevCapsA(i, &caps, sizeof(WAVEINCAPSA)) == MMSYSERR_NOERROR) {
                devices.emplace_back(caps.szPname);
            }
        }

        return devices;
    }

    bool startRecording(UINT deviceId = WAVE_MAPPER) {
        recordedData.clear();
        lastError.clear();
        dataReceived = false;

        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = wavHeader.numChannels;
        wfx.nSamplesPerSec = wavHeader.sampleRate;
        wfx.wBitsPerSample = wavHeader.bitsPerSample;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        wavHeader.byteRate = wfx.nAvgBytesPerSec;
        wavHeader.blockAlign = wfx.nBlockAlign;

        // Try opening the device with different sample rates if default fails
        const int sampleRates[] = { 44100, 48000, 22050, 16000, 8000 };
        MMRESULT result = MMSYSERR_ERROR;

        for (int rate : sampleRates) {
            wfx.nSamplesPerSec = rate;
            wfx.nAvgBytesPerSec = rate * wfx.nBlockAlign;

            result = waveInOpen(&hWaveIn, deviceId, &wfx,
                (DWORD_PTR)waveInProc, (DWORD_PTR)this,
                CALLBACK_FUNCTION);

            if (result == MMSYSERR_NOERROR) {
                wavHeader.sampleRate = rate;
                wavHeader.byteRate = wfx.nAvgBytesPerSec;
                break;
            }
        }

        if (result != MMSYSERR_NOERROR) {
            char errMsg[MAXERRORLENGTH];
            waveInGetErrorTextA(result, errMsg, MAXERRORLENGTH);
            lastError = "Failed to open audio device: " + std::string(errMsg);
            return false;
        }

        for (size_t i = 0; i < BUFFER_COUNT; i++) {
            waveHeaders[i].lpData = (LPSTR)audioBuffers[i].data();
            waveHeaders[i].dwBufferLength = BUFFER_SIZE;
            waveHeaders[i].dwFlags = 0;
            waveHeaders[i].dwBytesRecorded = 0;

            result = waveInPrepareHeader(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
            if (result != MMSYSERR_NOERROR) {
                lastError = "Failed to prepare buffer " + std::to_string(i);
                cleanup();
                return false;
            }

            result = waveInAddBuffer(hWaveIn, &waveHeaders[i], sizeof(WAVEHDR));
            if (result != MMSYSERR_NOERROR) {
                lastError = "Failed to add buffer " + std::to_string(i);
                cleanup();
                return false;
            }
        }

        isRecording = true;
        result = waveInStart(hWaveIn);
        if (result != MMSYSERR_NOERROR) {
            lastError = "Failed to start recording";
            cleanup();
            return false;
        }

        // Wait a short time to check if we're receiving data
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!dataReceived) {
            printf("Warning: No initial data received. Checking microphone access...\n");
            // Continue recording anyway, as data might start flowing
        }

        printf("Recording started successfully at %d Hz\n", wavHeader.sampleRate);
        return true;
    }

    void stopRecording() {
        if (!isRecording || !hWaveIn) return;

        isRecording = false;

        // Give a small delay to allow final buffers to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        waveInStop(hWaveIn);
        waveInReset(hWaveIn);
        cleanup();

        if (!recordedData.empty()) {
            writeWavFile();
            printf("Recording saved to: %s\n", outputFilename.c_str());
            printf("Recorded bytes: %zu\n", recordedData.size());
        }
        else {
            lastError = "No data was recorded. Please check if:\n"
                "1. Your microphone is properly connected\n"
                "2. The microphone has necessary permissions\n"
                "3. The microphone is not muted in Windows settings";
            printf("Warning: No data was recorded!\n");
        }
    }

    const std::string& getLastError() const {
        return lastError;
    }

private:
    void cleanup() {
        if (!hWaveIn) return;

        for (auto& header : waveHeaders) {
            if (header.dwFlags & WHDR_PREPARED) {
                waveInUnprepareHeader(hWaveIn, &header, sizeof(WAVEHDR));
            }
        }

        waveInClose(hWaveIn);
        hWaveIn = nullptr;
    }

    void writeWavFile() {
        wavHeader.dataSize = static_cast<uint32_t>(recordedData.size());
        wavHeader.riffSize = wavHeader.dataSize + sizeof(WAVHeader) - 8;

        std::ofstream file(outputFilename, std::ios::binary);
        if (!file) {
            lastError = "Failed to open output file: " + outputFilename;
            return;
        }

        file.write(reinterpret_cast<const char*>(&wavHeader), sizeof(WAVHeader));
        file.write(reinterpret_cast<const char*>(recordedData.data()),
            recordedData.size());
    }
};

int main() {
    printf("Checking available recording devices...\n");
    auto devices = AudioRecorder::listInputDevices();
    printf("Available recording devices:\n");
    for (size_t i = 0; i < devices.size(); i++) {
        printf("%zu: %s\n", i, devices[i].c_str());
    }

    AudioRecorder recorder("test.wav");

    if (!recorder.startRecording(0)) {
        printf("Error: %s\n", recorder.getLastError().c_str());
        return 1;
    }

    printf("Recording for 5 seconds...\n");
    Sleep(5000);

    recorder.stopRecording();

    if (!recorder.getLastError().empty()) {
        printf("Error occurred: %s\n", recorder.getLastError().c_str());
    }

    return 0;
}