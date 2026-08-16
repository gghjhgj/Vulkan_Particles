#include "FFT.h"

#include <cmath>
#include <stdexcept>

namespace
{

constexpr float PI =
    3.14159265358979323846f;

}

bool FFT::isPowerOfTwo(size_t value)
{
    return value != 0 &&
           (value & (value - 1)) == 0;
}

void FFT::forward(
    const float* input,
    std::vector<std::complex<float>>& output
)
{
    if (!input)
    {
        throw std::invalid_argument(
            "FFT: input is null"
        );
    }

    const size_t n =
        output.size();

    if (!isPowerOfTwo(n))
    {
        throw std::invalid_argument(
            "FFT: size must be a power of two"
        );
    }

    if (n == 1)
    {
        output[0] =
            std::complex<float>(
                input[0],
                0.0f
            );

        return;
    }

    size_t j = 0;

    for (size_t i = 1; i < n; ++i)
    {
        size_t bit =
            n >> 1;

        while (j & bit)
        {
            j ^= bit;
            bit >>= 1;
        }

        j ^= bit;

        if (i < j)
        {
            output[i] =
                std::complex<float>(
                    input[i],
                    0.0f
                );

            output[j] =
                std::complex<float>(
                    input[j],
                    0.0f
                );
        }
        else
        {
            output[i] =
                std::complex<float>(
                    input[i],
                    0.0f
                );
        }
    }

    output[0] =
        std::complex<float>(
            input[0],
            0.0f
        );

    for (size_t len = 2;
         len <= n;
         len <<= 1)
    {
        const float angle =
            -2.0f *
            PI /
            static_cast<float>(len);

        const std::complex<float> wLen(
            std::cos(angle),
            std::sin(angle)
        );

        for (size_t i = 0;
             i < n;
             i += len)
        {
            std::complex<float> w(
                1.0f,
                0.0f
            );

            const size_t half =
                len >> 1;

            for (size_t k = 0;
                 k < half;
                 ++k)
            {
                const auto u =
                    output[i + k];

                const auto v =
                    output[
                        i +
                        k +
                        half
                    ] * w;

                output[i + k] =
                    u + v;

                output[
                    i +
                    k +
                    half
                ] =
                    u - v;

                w *= wLen;
            }
        }
    }
}