/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "sbuf_stream.h"
#include "config.h"

/*
 * Stream interfaces
 */
sbuf_stream::sbuf_stream(const sbuf_t& sbuf_) : sbuf(sbuf_) { }
sbuf_stream::~sbuf_stream() {}
void sbuf_stream::seek(size_t offset_) { offset = offset_; }

size_t sbuf_stream::tell() { return offset; }

/*
 * unsigned integers, default little endian
 */
uint8_t sbuf_stream::get8u() {
    uint8_t value = sbuf.get8u(offset);
    offset++;
    return value;
}
uint16_t sbuf_stream::get16u() {
    uint16_t value = sbuf.get16u(offset);
    offset += 2;
    return value;
}
uint32_t sbuf_stream::get32u() {
    uint32_t value = sbuf.get32u(offset);
    offset += 4;
    return value;
}
uint64_t sbuf_stream::get64u() {
    uint64_t value = sbuf.get64u(offset);
    offset += 8;
    return value;
}

/*
 * unsigned integers, big endian
 */
uint8_t sbuf_stream::get8uBE() {
    uint8_t value = sbuf.get8uBE(offset);
    offset++;
    return value;
}
uint16_t sbuf_stream::get16uBE() {
    uint16_t value = sbuf.get16uBE(offset);
    offset += 2;
    return value;
}
uint32_t sbuf_stream::get32uBE() {
    uint32_t value = sbuf.get32uBE(offset);
    offset += 4;
    return value;
}
uint64_t sbuf_stream::get64uBE() {
    uint64_t value = sbuf.get64uBE(offset);
    offset += 8;
    return value;
}

/*
 * unsigned integers, byte order specified
 */
uint8_t sbuf_stream::get8u(sbuf_t::byte_order_t bo) {
    uint8_t value = sbuf.get8u(offset, bo);
    offset++;
    return value;
}
uint16_t sbuf_stream::get16u(sbuf_t::byte_order_t bo) {
    uint16_t value = sbuf.get16u(offset, bo);
    offset += 2;
    return value;
}
uint32_t sbuf_stream::get32u(sbuf_t::byte_order_t bo) {
    uint32_t value = sbuf.get32u(offset, bo);
    offset += 4;
    return value;
}
uint64_t sbuf_stream::get64u(sbuf_t::byte_order_t bo) {
    uint64_t value = sbuf.get64u(offset, bo);
    offset += 8;
    return value;
}

/*
 * signed integers, default little endian
 */
int8_t sbuf_stream::get8i() {
    int8_t value = sbuf.get8i(offset);
    offset++;
    return value;
}
int16_t sbuf_stream::get16i() {
    int16_t value = sbuf.get16i(offset);
    offset += 2;
    return value;
}
int32_t sbuf_stream::get32i() {
    int32_t value = sbuf.get32i(offset);
    offset += 4;
    return value;
}
int64_t sbuf_stream::get64i() {
    int64_t value = sbuf.get64i(offset);
    offset += 8;
    return value;
}

/*
 * signed integers, big endian
 */
int8_t sbuf_stream::get8iBE() {
    int8_t value = sbuf.get8iBE(offset);
    offset++;
    return value;
}
int16_t sbuf_stream::get16iBE() {
    int16_t value = sbuf.get16iBE(offset);
    offset += 2;
    return value;
}
int32_t sbuf_stream::get32iBE() {
    int32_t value = sbuf.get32iBE(offset);
    offset += 4;
    return value;
}
int64_t sbuf_stream::get64iBE() {
    int64_t value = sbuf.get64iBE(offset);
    offset += 8;
    return value;
}

/*
 * signed integers, byte order specified
 */
int8_t sbuf_stream::get8i(sbuf_t::byte_order_t bo) {
    int8_t value = sbuf.get8i(offset, bo);
    offset++;
    return value;
}
int16_t sbuf_stream::get16i(sbuf_t::byte_order_t bo) {
    int16_t value = sbuf.get16i(offset, bo);
    offset += 2;
    return value;
}
int32_t sbuf_stream::get32i(sbuf_t::byte_order_t bo) {
    int32_t value = sbuf.get32i(offset, bo);
    offset += 4;
    return value;
}
int64_t sbuf_stream::get64i(sbuf_t::byte_order_t bo) {
    int64_t value = sbuf.get64i(offset, bo);
    offset += 8;
    return value;
}

/*
 * string readers
 */
std::string sbuf_stream::getUTF8(size_t num_octets_requested)
{
    std::string utf8_string = sbuf.getUTF8(num_octets_requested);
    offset += utf8_string.length();
    return utf8_string;
}
std::string sbuf_stream::getUTF8() {
    std::string ret = sbuf.getUTF8(offset);
    size_t num_bytes = ret.length();
    // if anything was read then also skip \0
    if (num_bytes > 0) {
        num_bytes++;
    }
    offset += num_bytes;
    return ret;
}

std::wstring sbuf_stream::getUTF16(size_t code_units_requested) {
    std::wstring ret = sbuf.getUTF16(offset, code_units_requested);
    offset += ret.length() * 2;
    return ret;
}
std::wstring sbuf_stream::getUTF16() {
    std::wstring utf16_string = sbuf.getUTF16(offset);
    size_t num_bytes = utf16_string.length() * 2;
    if (num_bytes > 0) {
        // if anything was read then also skip \U0000
        num_bytes += 2;
    }
    offset += num_bytes;
    return utf16_string;
}
