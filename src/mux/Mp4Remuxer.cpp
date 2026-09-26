#include "mux/Mp4Remuxer.h"
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace Recorder::Mux {

    RemuxResult Mp4Remuxer::RemuxSync(const std::wstring& mkvPath, const std::wstring& mp4Path, bool deleteSourceOnSuccess) {
        auto startTime = std::chrono::high_resolution_clock::now();
        RemuxResult result;
        result.mp4Path = mp4Path;

        if (!std::filesystem::exists(mkvPath)) {
            result.success = false;
            result.errorMessage = L"Source MKV file does not exist";
            return result;
        }

#if defined(_WIN32)
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            result.success = false;
            result.errorMessage = L"Failed to initialize Media Foundation";
            return result;
        }

        IMFSourceReader* pReader = nullptr;
        IMFSinkWriter* pWriter = nullptr;
        bool remuxOk = false;

        hr = MFCreateSourceReaderFromURL(mkvPath.c_str(), nullptr, &pReader);
        if (SUCCEEDED(hr)) {
            hr = MFCreateSinkWriterFromURL(mp4Path.c_str(), nullptr, nullptr, &pWriter);
            if (SUCCEEDED(hr)) {
                DWORD streamIndex = 0;
                DWORD sinkStreamIndex = 0;
                IMFMediaType* pNativeType = nullptr;

                // Configure Video Stream
                if (SUCCEEDED(pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &pNativeType))) {
                    if (SUCCEEDED(pWriter->AddStream(pNativeType, &sinkStreamIndex))) {
                        pWriter->SetInputMediaType(sinkStreamIndex, pNativeType, nullptr);
                    }
                    pNativeType->Release();
                }

                if (SUCCEEDED(pWriter->BeginWriting())) {
                    while (true) {
                        DWORD flags = 0;
                        LONGLONG llTimestamp = 0;
                        IMFSample* pSample = nullptr;

                        hr = pReader->ReadSample(
                            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                            0,
                            &streamIndex,
                            &flags,
                            &llTimestamp,
                            &pSample
                        );

                        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                            if (pSample) pSample->Release();
                            break;
                        }

                        if (pSample) {
                            pWriter->WriteSample(sinkStreamIndex, pSample);
                            pSample->Release();
                        }
                    }

                    pWriter->Finalize();
                    remuxOk = true;
                }
                pWriter->Release();
            }
            pReader->Release();
        }

        MFShutdown();

        // If Media Foundation MKV reader wasn't installed or failed, copy/keep MKV file
        if (!remuxOk) {
            // Keep the pristine MKV as the primary recording
            result.mp4Path = mkvPath;
            result.success = true;
            result.fileSizeBytes = std::filesystem::file_size(mkvPath);
            return result;
        }
#else
        // Fallback for non-Windows mock builds
        std::filesystem::copy_file(mkvPath, mp4Path, std::filesystem::copy_options::overwrite_existing);
        bool remuxOk = true;
#endif

        if (remuxOk && deleteSourceOnSuccess && std::filesystem::exists(mp4Path)) {
            std::error_code ec;
            std::filesystem::remove(mkvPath, ec);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.remuxDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        result.fileSizeBytes = std::filesystem::file_size(mp4Path);
        result.success = true;
        return result;
    }

    void Mp4Remuxer::RemuxAsync(const std::wstring& mkvPath, const std::wstring& mp4Path, bool deleteSourceOnSuccess, CompletionCallback callback) {
        std::thread([mkvPath, mp4Path, deleteSourceOnSuccess, callback]() {
            RemuxResult res = RemuxSync(mkvPath, mp4Path, deleteSourceOnSuccess);
            if (callback) {
                callback(res);
            }
        }).detach();
    }

} // namespace Recorder::Mux
