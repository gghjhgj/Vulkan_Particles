#pragma once

#include <complex>
#include <cstddef>
#include <vector>

class FFT
{
public:
    static void forward(
        const float* input,
        std::vector<std::complex<float>>& output
    );

private:
    static bool isPowerOfTwo(
        size_t value
    );
};