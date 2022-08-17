/*
 * Copyright (c) 2022, Tom Needham <06needhamt@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Span.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/String.h>
#include <LibGfx/TGALoader.h>

namespace Gfx {

enum TGADataType : u8 {
    None = 0,
    UncompressedColorMapped = 1,
    UncompressedRGB = 2,
    UncompressedBlackAndWhite = 3,
    RunLengthEncodedColorMapped = 9,
    RunLengthEncodedRGB = 10,
    CompressedBlackAndWhite = 11,
    CompressedColorMapped = 32,
    CompressedColorMappedFourPass = 33
};

struct [[gnu::packed]] TGAHeader {
    u8 id_length;
    u8 color_map_type;
    TGADataType data_type_code;
    i16 color_map_origin;
    i16 color_map_length;
    u8 color_map_depth;
    i16 x_origin;
    i16 y_origin;
    u16 width;
    u16 height;
    u8 bits_per_pixel;
    u8 image_descriptor;
};

static_assert(sizeof(TGAHeader) == 18);

union [[gnu::packed]] TGAPixel {
    struct TGAColor {
        u8 blue;
        u8 green;
        u8 red;
        u8 alpha;
    } components;

    u32 data;
};

static_assert(AssertSize<TGAPixel, 4>());

class TGAReader {
public:
    TGAReader(ReadonlyBytes data)
        : m_data(move(data))
    {
    }

    ALWAYS_INLINE u8 read_u8()
    {
        u8 value = m_data[m_index];
        m_index++;
        return value;
    }

    ALWAYS_INLINE i8 read_i8()
    {
        return static_cast<i8>(read_u8());
    }

    ALWAYS_INLINE u16 read_u16()
    {
        return read_u8() | read_u8() << 8;
    }

    ALWAYS_INLINE i16 read_i16()
    {
        return read_i8() | read_i8() << 8;
    }

    ALWAYS_INLINE u32 read_u32()
    {
        return read_u16() | read_u16() << 16;
    }

    ALWAYS_INLINE i32 read_i32()
    {
        return read_i16() | read_i16() << 16;
    }

    ALWAYS_INLINE TGAPixel read_pixel(u8 bits_per_pixel)
    {
        auto pixel = TGAPixel();

        switch (bits_per_pixel) {
        case 24:
            pixel.components.blue = read_u8();
            pixel.components.green = read_u8();
            pixel.components.red = read_u8();
            pixel.components.alpha = 0xFF;
            return pixel;

        case 32:
            pixel.components.blue = read_u8();
            pixel.components.green = read_u8();
            pixel.components.red = read_u8();
            pixel.components.alpha = read_u8();
            return pixel;

        default:
            VERIFY_NOT_REACHED();
        }
    }

    size_t index() const
    {
        return m_index;
    }

    ReadonlyBytes data() const
    {
        return m_data;
    }

private:
    ReadonlyBytes m_data;
    size_t m_index { 0 };
};

struct TGALoadingContext {
    TGAHeader header;
    ReadonlyBytes bytes;
    size_t file_size;
    OwnPtr<TGAReader> reader = { nullptr };
    RefPtr<Gfx::Bitmap> bitmap;
};

TGAImageDecoderPlugin::TGAImageDecoderPlugin(u8 const* file_data, size_t file_size)
{
    m_context = make<TGALoadingContext>();
    m_context->bytes = ReadonlyBytes(file_data, file_size);
    m_context->file_size = move(file_size);
    m_context->reader = make<TGAReader>(m_context->bytes);
}

TGAImageDecoderPlugin::~TGAImageDecoderPlugin() = default;

IntSize TGAImageDecoderPlugin::size()
{
    return IntSize { m_context->header.width, m_context->header.height };
}

void TGAImageDecoderPlugin::set_volatile()
{
    if (m_context->bitmap)
        m_context->bitmap->set_volatile();
}

bool TGAImageDecoderPlugin::set_nonvolatile(bool& was_purged)
{
    if (!m_context->bitmap)
        return false;
    return m_context->bitmap->set_nonvolatile(was_purged);
}

bool TGAImageDecoderPlugin::decode_tga_header()
{
    auto& reader = m_context->reader;
    m_context->header = TGAHeader();
    m_context->header.id_length = reader->read_u8();
    m_context->header.color_map_type = reader->read_u8();
    m_context->header.data_type_code = static_cast<TGADataType>(reader->read_u8());
    m_context->header.color_map_origin = reader->read_i16();
    m_context->header.color_map_length = reader->read_i16();
    m_context->header.color_map_depth = reader->read_u8();
    m_context->header.x_origin = reader->read_i16();
    m_context->header.y_origin = reader->read_i16();
    m_context->header.width = reader->read_u16();
    m_context->header.height = reader->read_u16();
    m_context->header.bits_per_pixel = reader->read_u8();
    m_context->header.image_descriptor = reader->read_u8();

    auto bytes_remaining = reader->data().size() - reader->index();

    if (bytes_remaining < (m_context->header.width * m_context->header.height * (m_context->header.bits_per_pixel / 8)))
        return false;

    if (m_context->header.bits_per_pixel < 8 || m_context->header.bits_per_pixel > 32)
        return false;

    return true;
}

bool TGAImageDecoderPlugin::sniff()
{
    return decode_tga_header();
}

bool TGAImageDecoderPlugin::is_animated()
{
    return false;
}

size_t TGAImageDecoderPlugin::loop_count()
{
    return 0;
}

size_t TGAImageDecoderPlugin::frame_count()
{
    return 1;
}

ErrorOr<ImageFrameDescriptor> TGAImageDecoderPlugin::frame(size_t index)
{
    auto bits_per_pixel = m_context->header.bits_per_pixel;
    auto color_map = m_context->header.color_map_type;
    auto data_type = m_context->header.data_type_code;
    auto width = m_context->header.width;
    auto height = m_context->header.height;

    if (index != 0)
        return Error::from_string_literal("TGAImageDecoderPlugin: frame index must be 0");

    if (color_map > 1)
        return Error::from_string_literal("TGAImageDecoderPlugin: Invalid color map type");

    switch (bits_per_pixel) {
    case 24:
        m_context->bitmap = TRY(Bitmap::try_create(BitmapFormat::BGRx8888, { m_context->header.width, m_context->header.height }));
        break;

    case 32:
        m_context->bitmap = TRY(Bitmap::try_create(BitmapFormat::BGRA8888, { m_context->header.width, m_context->header.height }));
        break;

    default:
        // FIXME: Implement other TGA bit depths
        return Error::from_string_literal("TGAImageDecoderPlugin: Can only handle 24 and 32 bits per pixel");
    }
    switch (data_type) {
    case TGADataType::UncompressedRGB: {
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                auto pixel = m_context->reader->read_pixel(bits_per_pixel);
                m_context->bitmap->scanline(row)[col] = pixel.data;
            }
        }
        break;
    }
    default:
        // FIXME: Implement other TGA data types
        return Error::from_string_literal("TGAImageDecoderPlugin: Can currently only handle the UncompressedRGB data type");
    }

    VERIFY(m_context->bitmap);
    return ImageFrameDescriptor { m_context->bitmap, 0 };
}
}
