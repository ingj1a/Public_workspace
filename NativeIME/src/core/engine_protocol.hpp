#pragma once
#include "core/pinyin_engine.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace nativeime::core::ipc {
constexpr std::uint32_t Magic=0x454D494E; // NIME
constexpr std::uint16_t Version=1;
constexpr std::uint32_t MaxFrame=4096;
struct Request { std::wstring pinyin; };
struct Response { bool ok=false; std::vector<Candidate> candidates; };
std::vector<std::uint8_t> EncodeRequest(const Request& request);
bool DecodeRequest(const std::uint8_t* data,std::size_t size,Request& request) noexcept;
std::vector<std::uint8_t> EncodeResponse(const Response& response);
bool DecodeResponse(const std::uint8_t* data,std::size_t size,Response& response) noexcept;
}
