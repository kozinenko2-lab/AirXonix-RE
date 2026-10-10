#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include "legacy_mesh.hpp"
#include "core/cleanroom_archive.hpp"

// AXBM v1: portable, validated low-poly meshes loaded ONLY from clean-room ZIP.
// No original EXE/texture pack is needed for these native extras.
namespace airxonix::bonus {
inline bool loadModels(std::array<LegacyMesh,6>& models){
    std::vector<std::uint8_t> b;
    if(!CleanroomArchive::instance().read("models/bonus_models.axbm",b)||b.size()<8)return false;
    if(std::memcmp(b.data(),"AXBM",4)!=0)return false;
    std::size_t off=4;
    auto u8=[&](std::uint8_t& x){if(off>=b.size())return false;x=b[off++];return true;};
    auto u16=[&](std::uint16_t& x){if(off+2>b.size())return false;x=std::uint16_t(b[off])|std::uint16_t(b[off+1])<<8;off+=2;return true;};
    auto f32=[&](float& x){if(off+4>b.size())return false;std::uint32_t raw=std::uint32_t(b[off])|std::uint32_t(b[off+1])<<8|std::uint32_t(b[off+2])<<16|std::uint32_t(b[off+3])<<24;off+=4;std::memcpy(&x,&raw,4);return std::isfinite(x);};
    std::uint16_t ver=0,count=0;
    if(!u16(ver)||!u16(count)||ver!=1||count!=6)return false;
    std::array<LegacyMesh,6> parsed{};
    for(auto& m:parsed){
        std::uint16_t verts=0,faces=0;
        if(!u16(verts)||!u16(faces)||verts<3||verts>2048||faces<1||faces>4096)return false;
        m.vertices.reserve(verts);m.faces.reserve(faces);
        for(int i=0;i<verts;++i){LegacyMasterVertex v{};
            if(!f32(v.x)||!f32(v.y)||!f32(v.z)||!f32(v.nx)||!f32(v.ny)||!f32(v.nz)||!f32(v.u)||!f32(v.v))return false;
            if(std::fabs(v.x)>.05f||std::fabs(v.y)>.05f||std::fabs(v.z)>.05f||v.u<0.f||v.u>1.f||v.v<0.f||v.v>1.f)return false;
            m.vertices.push_back(v);
        }
        for(int i=0;i<faces;++i){std::uint8_t n=0;if(!u8(n)||(n!=3&&n!=4))return false;LegacyFace f{};for(int j=0;j<n;++j){std::uint16_t id=0;if(!u16(id)||id>=verts)return false;f.index.push_back(id);}m.faces.push_back(std::move(f));}
    }
    if(off!=b.size())return false;
    models=std::move(parsed);return true;
}
}
