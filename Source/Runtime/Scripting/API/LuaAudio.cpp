#include "EnginePCH.h"
#include "sol/sol.hpp"
#include "IO/FileUtils.h"

#include <objbase.h>
#include <xaudio2.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace
{
    struct FWaveClip
    {
        WAVEFORMATEX Format{};
        std::vector<BYTE> Data;
    };

    class FAudioSystem
    {
    public:
        ~FAudioSystem()
        {
            StopBGM();
            StopSFX();

            if (MasterVoice)
            {
                MasterVoice->DestroyVoice();
                MasterVoice = nullptr;
            }

            if (Engine)
            {
                Engine->Release();
                Engine = nullptr;
            }

            if (bCOMInitialized)
            {
                CoUninitialize();
                bCOMInitialized = false;
            }
        }

        bool PlayBGM(const FString& Path)
        {
            if (!Initialize())
                return false;

            FWaveClip* Clip = LoadWave(Path);
            if (!Clip)
                return false;

            StopBGM();

            IXAudio2SourceVoice* Voice = nullptr;

            if (FAILED(Engine->CreateSourceVoice(&Voice, &Clip->Format)))
                return false;

            XAUDIO2_BUFFER Buffer{};
            Buffer.AudioBytes = static_cast<UINT32>(Clip->Data.size());
            Buffer.pAudioData = Clip->Data.data();
            Buffer.Flags = XAUDIO2_END_OF_STREAM;
            Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

            if (FAILED(Voice->SubmitSourceBuffer(&Buffer)))
            {
                Voice->DestroyVoice();
                return false;
            }

            if (FAILED(Voice->Start(0)))
            {
                Voice->DestroyVoice();
                return false;
            }

            BGMVoice = Voice;
            return true;
        }

        bool PlaySFX(const FString& Path)
        {
            if (!Initialize())
                return false;

            FWaveClip* Clip = LoadWave(Path);
            if (!Clip)
                return false;

            CleanupFinishedSFX();

            IXAudio2SourceVoice* Voice = nullptr;

            if (FAILED(Engine->CreateSourceVoice(&Voice, &Clip->Format)))
                return false;

            XAUDIO2_BUFFER Buffer{};
            Buffer.AudioBytes = static_cast<UINT32>(Clip->Data.size());
            Buffer.pAudioData = Clip->Data.data();
            Buffer.Flags = XAUDIO2_END_OF_STREAM;

            if (FAILED(Voice->SubmitSourceBuffer(&Buffer)))
            {
                Voice->DestroyVoice();
                return false;
            }

            if (FAILED(Voice->Start(0)))
            {
                Voice->DestroyVoice();
                return false;
            }

            EffectVoices.push_back(Voice);
            return true;
        }

        void StopBGM()
        {
            if (!BGMVoice)
                return;

            BGMVoice->Stop(0);
            BGMVoice->FlushSourceBuffers();
            BGMVoice->DestroyVoice();
            BGMVoice = nullptr;
        }

        void StopSFX()
        {
            for (IXAudio2SourceVoice* Voice : EffectVoices)
            {
                if (!Voice)
                    continue;

                Voice->Stop(0);
                Voice->FlushSourceBuffers();
                Voice->DestroyVoice();
            }

            EffectVoices.clear();
        }

    private:
        bool Initialize()
        {
            if (Engine && MasterVoice)
                return true;

            if (!bCOMInitialized)
            {
                const HRESULT COMResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                if (FAILED(COMResult))
                    return false;

                bCOMInitialized = true;
            }

            if (FAILED(XAudio2Create(&Engine, 0, XAUDIO2_DEFAULT_PROCESSOR)))
            {
                Engine = nullptr;
                return false;
            }

            if (FAILED(Engine->CreateMasteringVoice(&MasterVoice)))
            {
                Engine->Release();
                Engine = nullptr;
                MasterVoice = nullptr;
                return false;
            }

            return true;
        }

        FWaveClip* LoadWave(const FString& AssetPath)
        {
            const FString FullPath = FileUtils::GetAssetFullPath(AssetPath);

            auto It = WaveCache.find(FullPath);
            if (It != WaveCache.end())
                return It->second.get();

            const std::filesystem::path NativePath =
                std::filesystem::u8path(FullPath.begin(), FullPath.end());

            std::ifstream File(NativePath, std::ios::binary);

            if (!File)
                return nullptr;

            File.seekg(0, std::ios::end);
            const std::streamoff FileSize = File.tellg();

            if (FileSize < 12)
                return nullptr;

            File.seekg(0, std::ios::beg);

            std::vector<BYTE> Bytes(static_cast<size_t>(FileSize));

            File.read(reinterpret_cast<char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));

            if (!File ||
                std::memcmp(Bytes.data(), "RIFF", 4) != 0 ||
                std::memcmp(Bytes.data() + 8, "WAVE", 4) != 0)
            {
                return nullptr;
            }

            const auto ReadU16 = [](const BYTE* P) -> WORD
                {
                    return static_cast<WORD>(P[0] | (static_cast<WORD>(P[1]) << 8));
                };

            const auto ReadU32 = [](const BYTE* P) -> DWORD
                {
                    return static_cast<DWORD>(P[0]) |
                        (static_cast<DWORD>(P[1]) << 8) |
                        (static_cast<DWORD>(P[2]) << 16) |
                        (static_cast<DWORD>(P[3]) << 24);
                };

            auto Clip = std::make_unique<FWaveClip>();

            bool bHasFormat = false;
            bool bHasData = false;
            size_t Offset = 12;

            while (Offset + 8 <= Bytes.size())
            {
                const BYTE* Header = Bytes.data() + Offset;
                const DWORD ChunkSize = ReadU32(Header + 4);

                Offset += 8;

                if (ChunkSize > Bytes.size() - Offset)
                    return nullptr;

                const BYTE* ChunkData = Bytes.data() + Offset;

                if (std::memcmp(Header, "fmt ", 4) == 0)
                {
                    if (ChunkSize < 16)
                        return nullptr;

                    Clip->Format.wFormatTag = ReadU16(ChunkData);
                    Clip->Format.nChannels = ReadU16(ChunkData + 2);
                    Clip->Format.nSamplesPerSec = ReadU32(ChunkData + 4);
                    Clip->Format.nAvgBytesPerSec = ReadU32(ChunkData + 8);
                    Clip->Format.nBlockAlign = ReadU16(ChunkData + 12);
                    Clip->Format.wBitsPerSample = ReadU16(ChunkData + 14);
                    Clip->Format.cbSize = 0;

                    if (Clip->Format.wFormatTag != WAVE_FORMAT_PCM &&
                        Clip->Format.wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
                    {
                        return nullptr;
                    }

                    bHasFormat = true;
                }
                else if (std::memcmp(Header, "data", 4) == 0)
                {
                    Clip->Data.assign(ChunkData, ChunkData + ChunkSize);
                    bHasData = true;
                }

                Offset += ChunkSize;

                // RIFF 청크 길이가 홀수이면 패딩 바이트가 붙는다.
                if (ChunkSize & 1)
                    ++Offset;
            }

            if (!bHasFormat ||
                !bHasData ||
                Clip->Data.empty() ||
                Clip->Format.nChannels == 0 ||
                Clip->Format.nSamplesPerSec == 0 ||
                Clip->Format.nBlockAlign == 0 ||
                Clip->Data.size() % Clip->Format.nBlockAlign != 0 ||
                Clip->Data.size() > std::numeric_limits<UINT32>::max())
            {
                return nullptr;
            }

            auto [Inserted, bSuccess] = WaveCache.emplace(FullPath, std::move(Clip));
            (void)bSuccess;

            return Inserted->second.get();
        }

        void CleanupFinishedSFX()
        {
            for (size_t i = 0; i < EffectVoices.size();)
            {
                XAUDIO2_VOICE_STATE State{};
                EffectVoices[i]->GetState(&State);

                if (State.BuffersQueued == 0)
                {
                    EffectVoices[i]->DestroyVoice();
                    EffectVoices.erase(EffectVoices.begin() + i);
                }
                else
                {
                    ++i;
                }
            }
        }

        IXAudio2* Engine = nullptr;
        IXAudio2MasteringVoice* MasterVoice = nullptr;
        IXAudio2SourceVoice* BGMVoice = nullptr;

        bool bCOMInitialized = false;

        std::unordered_map<FString, std::unique_ptr<FWaveClip>> WaveCache;
        std::vector<IXAudio2SourceVoice*> EffectVoices;
    };

    FAudioSystem& GetAudioSystem()
    {
        static FAudioSystem System;
        return System;
    }

    bool PlayBGM(const FString& Path)
    {
        return GetAudioSystem().PlayBGM(Path);
    }

    void StopBGM()
    {
        GetAudioSystem().StopBGM();
    }

    bool PlaySFX(const FString& Path)
    {
        return GetAudioSystem().PlaySFX(Path);
    }

    void StopSFX()
    {
        GetAudioSystem().StopSFX();
    }
}

void LuaRegisterAudio(sol::state& State)
{
    State.create_named_table("Audio", "PlayBGM", &PlayBGM, "StopBGM", &StopBGM, "PlaySFX", &PlaySFX, "StopSFX", &StopSFX);
}