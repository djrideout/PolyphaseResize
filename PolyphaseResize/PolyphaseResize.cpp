#include <windows.h>
#include <ppl.h>
#include <cmath>
#include "avisynth.h"

static const int COEFFS_LENGTH = 64;
// Scaling coefficients from 'Interpolation (Sharp).txt' in https://github.com/MiSTer-devel/Filters_MiSTer
static const int all_coeffs[COEFFS_LENGTH * 4] = {
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

class PolyphaseResize : public GenericVideoFilter {
public:
    PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env);
    ~PolyphaseResize();
    void Scale(byte* srcp, byte* dstp, int src_width, int dst_width, int dst_height, int src_x_scale, int src_y_scale, int dst_x_scale, int dst_y_scale);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int old_width;
    int old_height;
    byte* bufferp;
};

PolyphaseResize::PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env) : GenericVideoFilter(_child) {
    old_width = vi.width;
    old_height = vi.height;
    vi.width = _width;
    vi.height = _height;
    bufferp = new byte[_width * 4 * old_height];
    if (!vi.IsRGB32()) {
        env->ThrowError("PolyphaseResize: RGB32 data only!");
    }
}

PolyphaseResize::~PolyphaseResize() {
    delete[] bufferp;
}

void PolyphaseResize::Scale(byte* srcp, byte* dstp, int src_width, int dst_width, int dst_height, int src_x_scale, int src_y_scale, int dst_x_scale, int dst_y_scale) {
    concurrency::parallel_for(0, dst_height, [&](int y) {
        byte* s0 = srcp + y * src_y_scale;
        byte* d0 = dstp + y * dst_y_scale;
        for (int x = 0; x < dst_width; x++) {
            // The output pixel mapped onto the original source image
            double mapped_x = (static_cast<double>(x) / dst_width) * src_width;

            // 4 taps per phase, 64 phases
            int taps[4] = {
                mapped_x - 1.5,
                mapped_x - 0.5,
                mapped_x + 0.5,
                mapped_x + 1.5
            };

            // Clamp the pixel indices
            if (taps[0] < 0) taps[0] = 0;
            if (taps[1] < 0) taps[1] = 0;
            if (taps[2] >= src_width) taps[2] = src_width - 1;
            if (taps[3] >= src_width) taps[3] = src_width - 1;

            // Grab the pixel for each tap from the source image
            byte* pixels[4] = {
                s0 + taps[0] * src_x_scale,
                s0 + taps[1] * src_x_scale,
                s0 + taps[2] * src_x_scale,
                s0 + taps[3] * src_x_scale
            };

            // Get scaling coefficients for this pixel
            double phase = fmod((mapped_x + 0.5), 1) * COEFFS_LENGTH;
            const int* coeffs = &all_coeffs[static_cast<int>(phase) * 4];

            // Weigh the colours from each source pixel based on the coefficients for this phase to generate the result colour for this rendered pixel
            byte* dst_pixel = d0 + x * dst_x_scale;
            dst_pixel[0] = (pixels[0][0] * coeffs[0] + pixels[1][0] * coeffs[1] + pixels[2][0] * coeffs[2] + pixels[3][0] * coeffs[3]) >> 7;
            dst_pixel[1] = (pixels[0][1] * coeffs[0] + pixels[1][1] * coeffs[1] + pixels[2][1] * coeffs[2] + pixels[3][1] * coeffs[3]) >> 7;
            dst_pixel[2] = (pixels[0][2] * coeffs[0] + pixels[1][2] * coeffs[1] + pixels[2][2] * coeffs[2] + pixels[3][2] * coeffs[3]) >> 7;
            // This is the alpha byte, realistically these types of clips shouldn't have transparency and it's faster to not do this math
            dst_pixel[3] = 0;
        }
    });
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
    Scale(srcp, bufferp, src_rowsize >> 2, dst_rowsize >> 2, src_height, 4, src_pitch, 4, dst_rowsize);

    // Pass 2: Scale vertically from the buffer to the destination.
    Scale(bufferp, dstp, src_height, dst_height, dst_rowsize >> 2, dst_rowsize, 4, dst_pitch, 4);

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
