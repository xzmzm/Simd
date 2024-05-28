/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2024 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdSynetMergedConvolution16b.h"
#include "Simd/SimdSynetConvolution16bCommon.h"
#include "Simd/SimdBFloat16.h"
#include "Simd/SimdUpdate.h"
#include "Simd/SimdSse41.h"
#include "Simd/SimdCpu.h"

namespace Simd
{
#if defined(SIMD_SSE41_ENABLE) && defined(SIMD_SYNET_ENABLE) 
	namespace Sse41
	{
        using AlgParam = Base::SynetMergedConvolution16b::AlgParam;

        //-----------------------------------------------------------------------------------------

        static void ConvertFp32ToBf16(const uint8_t* src8, const ConvParam& p, const AlgParam& a, size_t yBeg, size_t yEnd, uint16_t* dst)
        {
            const float* src = (float*)src8;
            size_t srcC = AlignHi(p.srcC, a.miK);
            size_t bufH = a.bufH[0], mask = bufH - 1;
            if (srcC == p.srcC)
            {
                size_t size = p.srcW * p.srcC;
                size_t yInt = Simd::Max(yBeg, AlignLo(yEnd, bufH));
                if (yInt > yBeg)
                    Float32ToBFloat16(src + yBeg * size, (yInt - yBeg) * size, dst + (yBeg & mask) * size);
                if (yEnd > yInt)
                    Float32ToBFloat16(src + yInt * size, (yEnd - yInt) * size, dst + (yInt & mask) * size);
            }
            else
            {
                size_t srcC8 = Simd::AlignLo(p.srcC, 8);
                size_t srcC4 = Simd::AlignLo(p.srcC, 4);
                for (size_t y = yBeg; y < yEnd; ++y)
                {
                    const float* ps = src + y * p.srcW * p.srcC;
                    uint16_t* pd = dst + (y & mask) * p.srcW * srcC;
                    for (size_t x = 0; x < p.srcW; ++x)
                    {
                        size_t c = 0;
                        for (; c < srcC8; c += 8)
                        {
                            __m128i d0 = Float32ToBFloat16(_mm_loadu_ps(ps + c + 0));
                            __m128i d1 = Float32ToBFloat16(_mm_loadu_ps(ps + c + 4));
                            _mm_storeu_si128((__m128i*)(pd + c), _mm_packus_epi32(d0, d1));
                        }
                        for (; c < srcC4; c += 4)
                        {
                            __m128i d0 = Float32ToBFloat16(_mm_loadu_ps(ps + c + 0));
                            _mm_storel_epi64((__m128i*)(pd + c), _mm_packus_epi32(d0, K_ZERO));
                        }
                        for (; c < p.srcC; ++c)
                            pd[c] = Base::Float32ToBFloat16(ps[c]);
                        for (; c < srcC; ++c)
                            pd[c] = 0;
                        ps += p.srcC;
                        pd += srcC;
                    }
                }
            }
        }

        //-----------------------------------------------------------------------------------------

        SynetMergedConvolution16bCdc::SynetMergedConvolution16bCdc(const MergConvParam& p)
            : Base::SynetMergedConvolution16bCdc(p)
        {
            SetSize(F, 2);
            if(!_src16b)
                _convert = ConvertFp32ToBf16;
            SetInput(_param.conv[0], _input);
            SetDepthwise(_param.conv[1], _depthwise);
            SetOutput(_param.conv[2], _output);
        }

        //-----------------------------------------------------------------------------------------

        SynetMergedConvolution16bCd::SynetMergedConvolution16bCd(const MergConvParam& p)
            : Base::SynetMergedConvolution16bCd(p)
        {
            SetSize(F, 2);
            if (!_src16b)
                _convert = ConvertFp32ToBf16;
            SetInput(_param.conv[0], _input);
            SetDepthwise(_param.conv[1], _depthwise);
        }

        //-----------------------------------------------------------------------------------------

        SynetMergedConvolution16bDc::SynetMergedConvolution16bDc(const MergConvParam& p)
            : Base::SynetMergedConvolution16bDc(p)
        {
            SetSize(F, 2);
            SetDepthwise(_param.conv[0], _depthwise);
            SetOutput(_param.conv[1], _output);
        }

        //-----------------------------------------------------------------------------------------

        void* SynetMergedConvolution16bInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, SimdSynetCompatibilityType compatibility)
        {
            MergConvParam param(batch, convs, count, SimdFalse, compatibility);
            if (!param.Valid(SimdTensorData32f, SimdTensorData16b))
                return NULL;
            /*if (SynetMergedConvolution16bCdc::Preferable(param))
                return new Sse41::SynetMergedConvolution16bCdc(param);
            else */if (SynetMergedConvolution16bCd::Preferable(param))
                return new Sse41::SynetMergedConvolution16bCd(param);
            else if (SynetMergedConvolution16bDc::Preferable(param))
                return new Sse41::SynetMergedConvolution16bDc(param);
            else
                return new Base::SynetMergedConvolution16b(param);
        }
	}
#endif
}
