#include <windows.h>
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

    const unsigned char* srcp;
    unsigned char* dstp;
    int src_pitch, dst_pitch, row_size, height;
    int x, y;
    srcp = src->GetReadPtr();
    dstp = dst->GetWritePtr();

    src_pitch = src->GetPitch();
    dst_pitch = dst->GetPitch();
    row_size = dst->GetRowSize();
    height = dst->GetHeight();

    for (y = 0; y < height; y++) {
        for (x = 0; x < row_size; x++) {
            dstp[x] = srcp[x] ^ 255;
        }
        srcp += src_pitch;
        dstp += dst_pitch;
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
