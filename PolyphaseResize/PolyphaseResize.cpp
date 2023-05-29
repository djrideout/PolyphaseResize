#include <windows.h>
#include <cmath>
#include "avisynth.h"

class PolyphaseResize : public GenericVideoFilter {
public:
    PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int old_width;
    int old_height;
};

PolyphaseResize::PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env) : GenericVideoFilter(_child) {
    old_width = vi.width;
    old_height = vi.height;
    vi.width = _width;
    vi.height = _height;
    if (!vi.IsRGB32()) {
        env->ThrowError("PolyphaseResize: RGB32 data only!");
    }
}

PVideoFrame __stdcall PolyphaseResize::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrame(vi);

    const unsigned int* srcp;
    unsigned int* dstp;
    int src_width, dst_width, src_height, dst_height;
    int x, y;
    srcp = (unsigned int*)src->GetReadPtr();
    dstp = (unsigned int*)dst->GetWritePtr();

    src_width = src->GetPitch() / 4;
    dst_width = dst->GetPitch() / 4;
    src_height = src->GetHeight();
    dst_height = dst->GetHeight();

    for (y = 0; y < dst_height; y++) {
        for (x = 0; x < dst_width; x++) {
            // Pass 1: horizontal

            // The output pixel mapped onto the original source image
            float mapped_x = (float(x) / dst_width) * src_width;
            float mapped_y = (float(y) / dst_height) * src_height;

            // Get the mapped pixel to interpolate
            float to_scale = mapped_x;

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
            float phase = COEFFS_LENGTH * fmod((to_scale + 0.5), 1);
            int coeffs[4] = {
                all_coeffs[(int)phase * 4],
                all_coeffs[(int)phase * 4 + 1],
                all_coeffs[(int)phase * 4 + 2],
                all_coeffs[(int)phase * 4 + 3]
            };

            // 4 taps per phase, 64 phases
            int taps[4] = {
                to_scale - 1.5,
                to_scale - 0.5,
                to_scale + 0.5,
                to_scale + 1.5
            };

            // Determine which dimension to clamp with based on scaling direction
            int source_dimension = src_width;

            // Grab the pixel for each tap from the source image
            if (taps[0] < 0) taps[0] = 0;
            if (taps[1] < 0) taps[1] = 0;
            if (taps[2] >= source_dimension) taps[2] = source_dimension - 1;
            if (taps[3] >= source_dimension) taps[3] = source_dimension - 1;

            // Grab the pixel for each tap from the source image
            unsigned int pixels[4] = {
                srcp[taps[0]],
                srcp[taps[1]],
                srcp[taps[2]],
                srcp[taps[3]]
            };

            // Weigh the colours from each source pixel based on the coefficients for this phase to generate the result colour for this rendered pixel
            static const float lcm = 32640.0; // LCM of 128 and 255, as a float for casting
            byte* pixel0 = (byte*)&pixels[0];
            byte* pixel1 = (byte*)&pixels[1];
            byte* pixel2 = (byte*)&pixels[2];
            byte* pixel3 = (byte*)&pixels[3];
            byte* dst_pixel = (byte*)&dstp[x];
            dst_pixel[0] = (pixel0[0] * coeffs[0] + pixel1[0] * coeffs[1] + pixel2[0] * coeffs[2] + pixel3[0] * coeffs[3]) / lcm * 255;
            dst_pixel[1] = (pixel0[1] * coeffs[0] + pixel1[1] * coeffs[1] + pixel2[1] * coeffs[2] + pixel3[1] * coeffs[3]) / lcm * 255;
            dst_pixel[2] = (pixel0[2] * coeffs[0] + pixel1[2] * coeffs[1] + pixel2[2] * coeffs[2] + pixel3[2] * coeffs[3]) / lcm * 255;
            dst_pixel[3] = (pixel0[3] * coeffs[0] + pixel1[3] * coeffs[1] + pixel2[3] * coeffs[2] + pixel3[3] * coeffs[3]) / lcm * 255;
        }
        srcp += src_width;
        dstp += dst_width;
    }
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
