#pragma once

//=============================================================================
// range coder based on the Carryless rangecoder (c) 1999 by Dmitry Subbotin 
//=============================================================================


#include <cstdint>
#include <span>

#include <library/io.h>


namespace maniscalco::entropy
{

    template <io::stream_direction S>
    class range_encoder
    {
    public:

        range_encoder
        (
            io::push_stream<S> &
        );

        ~range_encoder();

        void flush();

        void encode
        (
            std::uint32_t cumFreq, 
            std::uint32_t freq, 
            std::uint32_t totFreq
        );

    private:

        io::push_stream<S> & encodeStream_;

        std::uint32_t   low_{0};
        
        std::uint32_t   range_{~0u};

        bool            flushRequired_{false};
    };


    using forward_range_encoder = range_encoder<io::stream_direction::forward>;
    using reverse_range_encoder = range_encoder<io::stream_direction::reverse>;

} // namespace maniscalco::entropy


//=============================================================================
template <maniscalco::io::stream_direction S>
maniscalco::entropy::range_encoder<S>::range_encoder
(
    io::push_stream<S> & encodeStream
):
    encodeStream_(encodeStream)
{
}


//=============================================================================
template <maniscalco::io::stream_direction S>
maniscalco::entropy::range_encoder<S>::~range_encoder
(
)
{
    flush();
}


//=============================================================================
template <maniscalco::io::stream_direction S>
void maniscalco::entropy::range_encoder<S>::flush
(
)
{
    if (flushRequired_)
    {
        for (auto i = 0; i < 4; ++i)
        {
            encodeStream_.push(low_ >> 24, 8);
            low_ <<= 8;
        }
        low_ = 0;
        range_ = ~0u;
        flushRequired_ = false;
    }
}


//=============================================================================
template <maniscalco::io::stream_direction S>
void maniscalco::entropy::range_encoder<S>::encode
(
    std::uint32_t cumFreq, 
    std::uint32_t freq, 
    std::uint32_t totFreq
) 
{
    static auto constexpr top = (1u << 24);
    static auto constexpr bot = (1u << 16);
    flushRequired_ = true;
    low_ += cumFreq * (range_ /= totFreq);
    range_ *= freq;


	while ( (low_ ^ low_+range_) < top || range_ < bot &&
			((range_ = -low_& bot-1), 1))
    {
        auto code = low_ >> 24;
        encodeStream_.push(code, 8); 
        range_ <<= 8;
        low_ <<= 8;
    }
}
