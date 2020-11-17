#include <library/entropy.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <string>
#include <queue>


namespace
{

    //==================================================================================================================
    std::vector<std::uint8_t> load_file
    (
        char const * path
    )
    {
        // read data from file
        std::vector<std::uint8_t> input;
        std::ifstream inputStream(path, std::ios_base::in | std::ios_base::binary);
        if (!inputStream.is_open())
        {
            std::cout << "failed to open file \"" << path << "\"" << std::endl;
            return {};
        }

        inputStream.seekg(0, std::ios_base::end);
        std::size_t size = inputStream.tellg();
        input.resize(size);
        inputStream.seekg(0, std::ios_base::beg);
        inputStream.read(reinterpret_cast<char *>(input.data()), input.size());
        inputStream.close();
        return input;
    }


    //=============================================================================
    template <maniscalco::io::stream_direction S>
    void range_encode 
    (
        std::span<std::uint8_t const> source,
        maniscalco::io::push_stream<S> & encodeStream
    )
    {
        static auto constexpr max_cumulative_total = 16384;
        std::array<std::uint32_t, 0x100> symbolCount{};
        auto cumulativeTotal = 0;
        for (auto c : source)
        {
            ++cumulativeTotal;
            if ((++symbolCount[c] >= 1023) || (cumulativeTotal >= max_cumulative_total))
            {
                cumulativeTotal = 0;
                for (auto & v : symbolCount)
                    cumulativeTotal += (v = ((v / 2) + (v == 1)));
            }
        }
        for (auto c : symbolCount)
            encodeStream.push(c, 10);

        std::uint32_t total = 0;
        for (auto & v : symbolCount)
        {
            total += v;
            v = total;
        }
        total += 1;

        maniscalco::entropy::range_encoder rangeEncoder(encodeStream);
        for (auto c : source)
        {
            auto start = (c > 0) ? symbolCount[c - 1] : 0;
            auto cur = symbolCount[c] - start;
            rangeEncoder.encode(start, cur, total);
        }
    }


    //=============================================================================
    template <maniscalco::io::stream_direction S>
    void range_decode 
    (
        std::span<std::uint8_t> destination,
        maniscalco::io::pop_stream<S> & decodeStream
    )
    {
        struct symbol_info
        {
            std::uint32_t freq_;
            std::uint8_t symbol_;
        };

        std::array<symbol_info, 0x100> symbolCount;
        std::uint8_t v = 0;
        for (auto & c : symbolCount)
            c = {(std::uint32_t)decodeStream.pop(10), v++};

        std::uint32_t total = 0;
        for (auto & [c, _]: symbolCount)
        {
            total += c;
            c = total;
        }
        total += 1;

        maniscalco::entropy::range_decoder rangeDecoder(decodeStream);
        for (auto & out : destination)
        {
            auto f = rangeDecoder.get_current_frequency(total);
            auto a = 0;
            auto b = 0x100;
            auto m = (b + a) / 2;
            while (a < b)
            {
                if (symbolCount[m].freq_ <= f)
                    a = m + 1;
                else
                    b = m;
                m = (a + b) / 2;
            }
            while ((m > 0) && (symbolCount[m - 1].freq_ == symbolCount[m].freq_))
                --m;
            auto [c, v] = symbolCount[m];
            auto start = (m > 0) ? symbolCount[m - 1].freq_ : 0;
            auto cur = c - start;
            rangeDecoder.decode(start, cur, total);
            out = v;
        }
    }


    //==================================================================================================================
    template <maniscalco::io::stream_direction S>
    void encode_and_decode
    (
        std::span<std::uint8_t> source
    )
    {        
        std::uint32_t inputSize = source.size();

        std::deque<typename maniscalco::io::push_stream<S>::packet_type> data;

        maniscalco::io::push_stream<S> encodeStream(
            {
                .bufferOutputHandler_ = [&](auto packet)
                {
                    data.emplace_back(std::move(packet));
                }
            });
        auto startTime = std::chrono::system_clock::now();
        range_encode(source, encodeStream);
        encodeStream.flush();
        auto finishTime = std::chrono::system_clock::now();
        auto elapsedOverallEncode = std::chrono::duration_cast<std::chrono::milliseconds>(finishTime - startTime).count();
        auto outputSize = encodeStream.size() / 8;
        std::cout << "compressed: " << inputSize << " -> " << outputSize << " bytes.  ratio = " << (((long double)outputSize / inputSize) * 100) << "%" << std::endl;
        std::cout << "Elapsed time: " << ((long double)elapsedOverallEncode / 1000) << " seconds : " <<  (((long double)inputSize / (1 << 20)) / ((double)elapsedOverallEncode / 1000)) << " MB/sec" << std::endl;


        maniscalco::io::pop_stream<S> decodeStream(
            {
                .inputHandler_ = [&]()
                {
                    auto packet = std::move(data.front());
                    data.pop_front();
                    return packet;
                }
            });
        std::vector<std::uint8_t> decodedData;
        decodedData.resize(inputSize);
        inputSize = ((encodeStream.size() + 7) / 8);
        startTime = std::chrono::system_clock::now();
        range_decode({decodedData.begin(), decodedData.end()}, decodeStream);
        finishTime = std::chrono::system_clock::now();
        elapsedOverallEncode = std::chrono::duration_cast<std::chrono::milliseconds>(finishTime - startTime).count();
        outputSize = decodedData.size();
        std::cout << "decompressed: " << inputSize << " -> " << outputSize << " bytes" << std::endl;
        std::cout << "Elapsed time: " << ((long double)elapsedOverallEncode / 1000) << " seconds : " <<  (((long double)inputSize / (1 << 20)) / ((double)elapsedOverallEncode / 1000)) << " MB/sec" << std::endl;

        if (std::vector<std::uint8_t>(source.begin(), source.end()) != decodedData)
            std::cout << "*** ERROR DETECTED ***" << std::endl;
        else 
            std::cout << "range coder output validated" << std::endl;
    }


    //==================================================================================================================
    void print_about
    (
    )
    {
        std::cout << "Adaptation of the Carryless rangecoder (c) 1999 by Dmitry Subbotin" << std::endl;
        std::cout << "Author: M.A. Maniscalco" << std::endl;
    }


    //==================================================================================================================
    std::int32_t print_usage
    (
    )
    {
        std::cout << "Usage: range_coder_demo inputFile" << std::endl;
        return 0;
    }
}


//======================================================================================================================
std::int32_t main
(
    std::int32_t argCount,
    char const * argValue[]
)
{
    print_about();

    if (argCount != 2)
        return print_usage();

    auto input = load_file(argValue[1]);
    encode_and_decode<maniscalco::io::stream_direction::forward>({input.begin(), input.end()});

    return 0;
}

