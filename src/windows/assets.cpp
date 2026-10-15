// WIC decoder -> format converter -> checked top-down pixels follows Microsoft's
// WicViewerGdi sample at 434f6002bdf9cf9829406c3ff2b33387982d6168.
// Conversion is PBGRA (not sample BGR); scaling is handled by the core renderer.
// See third_party/Windows-classic-samples-LICENSE.txt.
#include "windows/assets.hpp"
#include <windows.h>
#include <wincodec.h>
#include <bcrypt.h>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace pet {
namespace {
template<class T> struct Release { void operator()(T* p) const { if (p) p->Release(); } };
template<class T> using Com = std::unique_ptr<T, Release<T>>;
void hrCheck(HRESULT hr, const char* operation, const std::string& key) {
    if (FAILED(hr)) {
        std::ostringstream s;
        s << operation << " assetKey=" << key << " HRESULT=0x" << std::hex << uint32_t(hr);
        throw std::runtime_error(s.str());
    }
}
std::string sha256(const std::vector<uint8_t>& data) {
    std::array<uint8_t, 32> digest{};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider NTSTATUS=" + std::to_string(status));
    status = BCryptHash(algorithm, nullptr, 0,
        const_cast<PUCHAR>(data.data()), ULONG(data.size()), digest.data(), ULONG(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("BCryptHash SHA-256 NTSTATUS=" + std::to_string(status));
    std::ostringstream s;
    for (auto b : digest) s << std::hex << std::setw(2) << std::setfill('0') << unsigned(b);
    return s.str();
}
}
Assets Assets::load(const std::filesystem::path& manifest, const Assets* previous) {
    Assets result;
    result.package = loadPackage(manifest);
    const auto& p = result.package;
    auto png = readFile(manifest.parent_path() / std::filesystem::u8path(p.atlasName), 8 * 1024 * 1024);
    if (png.size() != p.pngBytes || sha256(png) != p.pngSha256)
        throw std::runtime_error("atlas size/SHA-256 assetKey=" + p.assetKey);
    if (previous && previous->package.pngSha256 == p.pngSha256 && previous->atlas->width == p.width && previous->atlas->height == p.height) {
        result.atlas = previous->atlas;
        return result;
    }
    IWICImagingFactory* rawFactory = nullptr;
    hrCheck(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawFactory)), "WIC factory", p.assetKey);
    Com<IWICImagingFactory> factory(rawFactory);
    IWICStream* rawStream = nullptr;
    hrCheck(factory->CreateStream(&rawStream), "WIC stream", p.assetKey);
    Com<IWICStream> stream(rawStream);
    hrCheck(stream->InitializeFromMemory(png.data(), DWORD(png.size())), "WIC memory stream", p.assetKey);
    IWICBitmapDecoder* rawDecoder = nullptr;
    hrCheck(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnLoad, &rawDecoder), "PNG decode", p.assetKey);
    Com<IWICBitmapDecoder> decoder(rawDecoder);
    GUID format{};
    hrCheck(decoder->GetContainerFormat(&format), "PNG format", p.assetKey);
    if (format != GUID_ContainerFormatPng) throw std::runtime_error("atlas is not PNG assetKey=" + p.assetKey);
    UINT count = 0;
    hrCheck(decoder->GetFrameCount(&count), "PNG frame count", p.assetKey);
    if (count != 1) throw std::runtime_error("atlas frame count assetKey=" + p.assetKey);
    IWICBitmapFrameDecode* rawFrame = nullptr;
    hrCheck(decoder->GetFrame(0, &rawFrame), "PNG frame", p.assetKey);
    Com<IWICBitmapFrameDecode> frame(rawFrame);
    UINT width = 0, height = 0;
    hrCheck(frame->GetSize(&width, &height), "PNG size", p.assetKey);
    if (width != p.width || height != p.height) throw std::runtime_error("atlas dimensions assetKey=" + p.assetKey);
    IWICFormatConverter* rawConverter = nullptr;
    hrCheck(factory->CreateFormatConverter(&rawConverter), "WIC converter", p.assetKey);
    Com<IWICFormatConverter> converter(rawConverter);
    hrCheck(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "WIC PBGRA", p.assetKey);
    result.atlas = std::make_shared<Surface>(width, height);
    hrCheck(converter->CopyPixels(nullptr, width * 4, UINT(result.atlas->pixels.size()), result.atlas->pixels.data()), "WIC CopyPixels", p.assetKey);
    result.atlas->validatePbgra();
    result.decoded = true;
    return result;
}
}
