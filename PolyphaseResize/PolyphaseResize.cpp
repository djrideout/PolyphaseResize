#include <windows.h>
#include "avisynth.h"

class PolyphaseResize : public GenericVideoFilter {
public:
    PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int width;
    int height;
};

PolyphaseResize::PolyphaseResize(PClip _child, int _width, int _height, IScriptEnvironment* env) :
    GenericVideoFilter(_child),
    width(_width),
    height(_height)  {
    if (!vi.IsPlanar() || !vi.IsYUV()) {
        env->ThrowError("PolyphaseResize: planar YUV data only!");
    }
}

PVideoFrame __stdcall PolyphaseResize::GetFrame(int n, IScriptEnvironment* env) {
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = env->NewVideoFrame(vi);

    const unsigned char* srcp;
    unsigned char* dstp;
    int src_pitch, dst_pitch, row_size, height;
    int p, x, y;

    int planes[] = { PLANAR_Y, PLANAR_V, PLANAR_U };

    for (p = 0; p < 3; p++) {
        srcp = src->GetReadPtr(planes[p]);
        dstp = dst->GetWritePtr(planes[p]);

        src_pitch = src->GetPitch(planes[p]);
        dst_pitch = dst->GetPitch(planes[p]);
        row_size = dst->GetRowSize(planes[p]);
        height = dst->GetHeight(planes[p]);

        for (y = 0; y < height; y++) {
            for (x = 0; x < row_size; x++) {
                dstp[x] = srcp[x] ^ 255;
            }
            srcp += src_pitch; // or srcp = srcp + src_pitch;
            dstp += dst_pitch; // or dstp = dstp + dst_pitch;
        }
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
