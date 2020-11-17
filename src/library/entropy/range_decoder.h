#pragma once

//=============================================================================
// range coder based on the Carryless rangecoder (c) 1999 by Dmitry Subbotin 
//=============================================================================

#include <library/io.h>

#include <span>
#include <cstdint>


namespace maniscalco::entropy
{

    template <io::stream_direction S>
    class range_decoder
    {
    public:

        range_decoder
        (
            io::pop_stream<S> &
        );

        void decode
        (
            std::uint32_t, 
            std::uint32_t, 
            std::uint32_t
        );

        std::uint32_t get_current_frequency
        (
            std::uint32_t
        );

    private:

        io::pop_stream<S> & decodeStream_;

        std::uint32_t   low_{0};

        std::uint32_t   code_{0};
        
        std::uint32_t   range_{~0u};
    };


    using forward_range_decoder = range_decoder<io::stream_direction::forward>;
    using reverse_range_decoder = range_decoder<io::stream_direction::reverse>;

} // namespace maniscalco::entropy


//=============================================================================
template <maniscalco::io::stream_direction S>
maniscalco::entropy::range_decoder<S>::range_decoder
(
    io::pop_stream<S> & decodeStream
):
    decodeStream_(decodeStream)
{
    for (auto i = 0; i < 4; ++i)
    {
        code_ <<= 8;
        code_ |= decodeStream_.pop(8);
    }
}


//=============================================================================
template <maniscalco::io::stream_direction S>
inline void maniscalco::entropy::range_decoder<S>::decode 
(
    std::uint32_t cumFreq, 
    std::uint32_t freq, 
    std::uint32_t totFreq
)
{
    static auto constexpr top = (1u << 24);
    static auto constexpr bot = (1u << 16);
    low_ += (cumFreq * range_);
    range_ *= freq;

	while ( (low_ ^ low_+range_) < top || range_ < bot &&
			((range_ = -low_& bot-1), 1) )
    {
        code_ <<= 8;
        auto code = decodeStream_.pop(8);
        code_ |= code;
        range_ <<= 8;
        low_ <<= 8;
    }
}


//=============================================================================
template <maniscalco::io::stream_direction S>
inline std::uint32_t maniscalco::entropy::range_decoder<S>::get_current_frequency
(
    std::uint32_t total
) 
{
    return (code_ - low_) / (range_ /= total);
}
