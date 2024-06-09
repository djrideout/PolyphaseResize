#include <windows.h>
#include <cmath>
#include "avisynth.h"

class PolyphaseResize : public GenericVideoFilter {
public:
    PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env);
    ~PolyphaseResize();
    void TurnLeft(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch);
    void TurnRight(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch);
    void ScaleHorizontal(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_rowsize, int dst_pitch);
    void ScaleVertical(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch, int dst_height);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int old_width;
    int old_height;
    byte* horizontal_bufferp;
    byte* turn_bufferp;
    byte* scale_bufferp;
};

PolyphaseResize::PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env) : GenericVideoFilter(_child) {
    old_width = vi.width;
    old_height = vi.height;
    vi.width = _width;
    vi.height = _height;
    horizontal_bufferp = new byte[_width * 4 * old_height];
    turn_bufferp = new byte[_width * 4 * old_height];
    scale_bufferp = new byte[_width * 4 * _height];
    if (!vi.IsRGB32()) {
        env->ThrowError("PolyphaseResize: RGB32 data only!");
    }
}

PolyphaseResize::~PolyphaseResize() {
    delete[] horizontal_bufferp;
    delete[] turn_bufferp;
    delete[] scale_bufferp;
}

/**
 * From turn_right_plane_c in AviSynth (Right and left turn are swapped in RGB32)
 */
void PolyphaseResize::TurnLeft(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch) {
    byte* s0 = srcp + src_pitch * (src_height - 1);
    for (int y = 0; y < src_height; y++) {
        byte* d0 = dstp;
        for (int x = 0; x < src_rowsize; x += 4) {
            *reinterpret_cast<unsigned int*>(d0) = *reinterpret_cast<unsigned int*>(s0 + x);
            d0 += dst_pitch;
        }
        s0 -= src_pitch;
        dstp += 4;
    }
}

/**
 * From turn_left_plane_32_c in AviSynth (Right and left turn are swapped in RGB32)
 */
void PolyphaseResize::TurnRight(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch) {
    TurnLeft(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / 4 - 1), src_rowsize, -src_pitch, src_height, -dst_pitch);
}

void PolyphaseResize::ScaleHorizontal(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_rowsize, int dst_pitch) {
    int src_rowsize_uints = src_rowsize / 4;
    int dst_rowsize_uints = dst_rowsize / 4;

    for (int y = 0; y < src_height; y++) {
        for (int x = 0; x < dst_rowsize_uints; x++) {
            // The output pixel mapped onto the original source image
            double mapped_x = (static_cast<double>(x) / dst_rowsize_uints) * src_rowsize_uints;

            // Get scaling coefficients for this pixel
            static const int COEFFS_LENGTH = 64;
            // Scaling coefficients from 'Interpolation (Sharp).txt' in https://github.com/MiSTer-devel/Filters_MiSTer
            static int all_coeffs[COEFFS_LENGTH * 4] = {
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 128,   0,   0,
                0, 127,   1,   0,
                0, 127,   1,   0,
                0, 127,   1,   0,
                0, 127,   1,   0,
                0, 126,   2,   0,
                0, 125,   3,   0,
                0, 124,   4,   0,
                0, 123,   5,   0,
                0, 121,   7,   0,
                0, 119,   9,   0,
                0, 116,  12,   0,
                0, 112,  16,   0,
                0, 107,  21,   0,
                0, 100,  28,   0,
                0,  93,  35,   0,
                0,  84,  44,   0,
                0,  74,  54,   0,
                0,  64,  64,   0,
                0,  54,  74,   0,
                0,  44,  84,   0,
                0,  35,  93,   0,
                0,  28, 100,   0,
                0,  21, 107,   0,
                0,  16, 112,   0,
                0,  12, 116,   0,
                0,   9, 119,   0,
                0,   7, 121,   0,
                0,   5, 123,   0,
                0,   4, 124,   0,
                0,   3, 125,   0,
                0,   2, 126,   0,
                0,   1, 127,   0,
                0,   1, 127,   0,
                0,   1, 127,   0,
                0,   1, 127,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0,
                0,   0, 128,   0
            };
            double phase = COEFFS_LENGTH * fmod((mapped_x + 0.5), 1);
            int coeffs[4] = {
                all_coeffs[static_cast<int>(phase) * 4],
                all_coeffs[static_cast<int>(phase) * 4 + 1],
                all_coeffs[static_cast<int>(phase) * 4 + 2],
                all_coeffs[static_cast<int>(phase) * 4 + 3]
            };

            // 4 taps per phase, 64 phases
            int taps[4] = {
                mapped_x - 1.5,
                mapped_x - 0.5,
                mapped_x + 0.5,
                mapped_x + 1.5
            };

            // Grab the pixel for each tap from the source image
            if (taps[0] < 0) taps[0] = 0;
            if (taps[1] < 0) taps[1] = 0;
            if (taps[2] >= src_rowsize_uints) taps[2] = src_rowsize_uints - 1;
            if (taps[3] >= src_rowsize_uints) taps[3] = src_rowsize_uints - 1;

            // Grab the pixel for each tap from the source image
            unsigned int pixels[4] = {
                reinterpret_cast<unsigned int*>(srcp)[taps[0]],
                reinterpret_cast<unsigned int*>(srcp)[taps[1]],
                reinterpret_cast<unsigned int*>(srcp)[taps[2]],
                reinterpret_cast<unsigned int*>(srcp)[taps[3]]
            };

            // Weigh the colours from each source pixel based on the coefficients for this phase to generate the result colour for this rendered pixel
            static const double lcm = 32640.0; // LCM of 128 and 255, as a double for casting
            byte* pixel0 = reinterpret_cast<byte*>(&pixels[0]);
            byte* pixel1 = reinterpret_cast<byte*>(&pixels[1]);
            byte* pixel2 = reinterpret_cast<byte*>(&pixels[2]);
            byte* pixel3 = reinterpret_cast<byte*>(&pixels[3]);
            byte* dst_pixel = &dstp[x * 4];
            dst_pixel[0] = (pixel0[0] * coeffs[0] + pixel1[0] * coeffs[1] + pixel2[0] * coeffs[2] + pixel3[0] * coeffs[3]) / lcm * 255;
            dst_pixel[1] = (pixel0[1] * coeffs[0] + pixel1[1] * coeffs[1] + pixel2[1] * coeffs[2] + pixel3[1] * coeffs[3]) / lcm * 255;
            dst_pixel[2] = (pixel0[2] * coeffs[0] + pixel1[2] * coeffs[1] + pixel2[2] * coeffs[2] + pixel3[2] * coeffs[3]) / lcm * 255;
            dst_pixel[3] = (pixel0[3] * coeffs[0] + pixel1[3] * coeffs[1] + pixel2[3] * coeffs[2] + pixel3[3] * coeffs[3]) / lcm * 255;
        }
        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

void PolyphaseResize::ScaleVertical(byte* srcp, byte* dstp, int src_rowsize, int src_pitch, int src_height, int dst_pitch, int dst_height) {
    // Turn source left for to prepare for horizontal scaling
    TurnLeft(srcp, turn_bufferp, src_rowsize, src_pitch, src_height, src_height * 4);

    // Scale the turned source horizontally
    ScaleHorizontal(turn_bufferp, scale_bufferp, src_height * 4, src_height * 4, src_rowsize / 4, dst_height * 4, dst_height * 4);

    // Turn scaled buffer right into the destination buffer
    TurnRight(scale_bufferp, dstp, dst_height * 4, dst_height * 4, src_rowsize / 4, dst_pitch);
}

PVideoFrame __stdcall PolyphaseResize::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrame(vi);

    int src_rowsize = src->GetRowSize();
    int dst_rowsize = dst->GetRowSize();
    int src_pitch = src->GetPitch();
    int dst_pitch = dst->GetPitch();
    int src_height = src->GetHeight();
    int dst_height = dst->GetHeight();
    byte* srcp = const_cast<byte*>(src->GetReadPtr());
    byte* dstp = static_cast<byte*>(dst->GetWritePtr());

    // Pass 1: Scale horizontally from the source to the buffer.
    ScaleHorizontal(srcp, horizontal_bufferp, src_rowsize, src_pitch, src_height, dst_rowsize, dst_rowsize);

    // Pass 2: Scale vertically from the buffer to the destination.
    ScaleVertical(horizontal_bufferp, dstp, dst_rowsize, dst_rowsize, src_height, dst_pitch, dst_height);

    return dst;
}

AVSValue __cdecl Create_PolyphaseResize(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new PolyphaseResize(args[0].AsClip(), args[1].AsInt(), args[2].AsInt(), env);
}

const AVS_Linkage* AVS_linkage = 0;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors) {
    AVS_linkage = vectors;
    env->AddFunction("PolyphaseResize", "cii", Create_PolyphaseResize, 0);
    return "Filter for scaling pixel-perfect sources";
}
