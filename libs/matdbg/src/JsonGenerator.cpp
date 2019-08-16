/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <filaflat/BlobDictionary.h>
#include <filaflat/ChunkContainer.h>
#include <filaflat/DictionaryReader.h>
#include <filaflat/MaterialChunk.h>
#include <filaflat/ShaderBuilder.h>
#include <filaflat/Unflattener.h>

#include <filament/MaterialChunkType.h>
#include <filament/MaterialEnums.h>

#include <backend/DriverEnums.h>

#include "ShaderInfo.h"
#include "JsonGenerator.h"

#include <sstream>

using namespace filament;
using namespace backend;
using namespace filaflat;
using namespace filamat;
using namespace std;
using namespace utils;

namespace filament {
namespace matdbg {

template<typename T>
static bool read(const ChunkContainer& container, filamat::ChunkType type, T* value) noexcept {
    if (!container.hasChunk(type)) {
        return false;
    }

    Unflattener unflattener(container.getChunkStart(type), container.getChunkEnd(type));
    return unflattener.read(value);
}


template<typename T>
static const char* toString(T value);

template<>
const char* toString(Shading shadingModel) {
    switch (shadingModel) {
        case Shading::UNLIT: return "unlit";
        case Shading::LIT: return "lit";
        case Shading::SUBSURFACE: return "subsurface";
        case Shading::CLOTH: return "cloth";
        case Shading::SPECULAR_GLOSSINESS: return "specularGlossiness";
    }
}

template<>
const char* toString(BlendingMode blendingMode) {
    switch (blendingMode) {
        case BlendingMode::OPAQUE: return "opaque";
        case BlendingMode::TRANSPARENT: return "transparent";
        case BlendingMode::ADD: return "add";
        case BlendingMode::MASKED: return "masked";
        case BlendingMode::FADE: return "fade";
        case BlendingMode::MULTIPLY: return "multiply";
        case BlendingMode::SCREEN: return "screen";
    }
}

template<>
const char* toString(Interpolation interpolation) {
    switch (interpolation) {
        case Interpolation::SMOOTH: return "smooth";
        case Interpolation::FLAT: return "flat";
    }
}

template<>
const char* toString(VertexDomain domain) {
    switch (domain) {
        case VertexDomain::OBJECT: return "object";
        case VertexDomain::WORLD: return "world";
        case VertexDomain::VIEW: return "view";
        case VertexDomain::DEVICE: return "device";
    }
}

template<>
const char* toString(CullingMode cullingMode) {
    switch (cullingMode) {
        case CullingMode::NONE: return "none";
        case CullingMode::FRONT: return "front";
        case CullingMode::BACK: return "back";
        case CullingMode::FRONT_AND_BACK: return "front & back";
    }
}

template<>
const char* toString(TransparencyMode transparencyMode) {
    switch (transparencyMode) {
        case TransparencyMode::DEFAULT: return "default";
        case TransparencyMode::TWO_PASSES_ONE_SIDE: return "two passes, one side";
        case TransparencyMode::TWO_PASSES_TWO_SIDES: return "two passes, two sides";
    }
}

template<>
const char* toString(VertexAttribute attribute) {
    switch (attribute) {
        case VertexAttribute::POSITION: return "position";
        case VertexAttribute::TANGENTS: return "tangents";
        case VertexAttribute::COLOR: return "color";
        case VertexAttribute::UV0: return "uv0";
        case VertexAttribute::UV1: return "uv1";
        case VertexAttribute::BONE_INDICES: return "bone indices";
        case VertexAttribute::BONE_WEIGHTS: return "bone weights";
        case VertexAttribute::CUSTOM0: return "custom0";
        case VertexAttribute::CUSTOM1: return "custom1";
        case VertexAttribute::CUSTOM2: return "custom2";
        case VertexAttribute::CUSTOM3: return "custom3";
        case VertexAttribute::CUSTOM4: return "custom4";
        case VertexAttribute::CUSTOM5: return "custom5";
        case VertexAttribute::CUSTOM6: return "custom6";
        case VertexAttribute::CUSTOM7: return "custom7";
    }
    return "--";
}

template<>
const char* toString(bool value) {
    return value ? "true" : "false";
}

template<>
const char* toString(ShaderType stage) {
    switch (stage) {
        case ShaderType::VERTEX: return "vs";
        case ShaderType::FRAGMENT: return "fs";
        default: break;
    }
    return "--";
}

template<>
const char* toString(ShaderModel model) {
    switch (model) {
        case ShaderModel::UNKNOWN: return "--";
        case ShaderModel::GL_ES_30: return "gles30";
        case ShaderModel::GL_CORE_41: return "gl41";
    }
}

template<>
const char* toString(UniformType type) {
    switch (type) {
        case UniformType::BOOL:   return "bool";
        case UniformType::BOOL2:  return "bool2";
        case UniformType::BOOL3:  return "bool3";
        case UniformType::BOOL4:  return "bool4";
        case UniformType::FLOAT:  return "float";
        case UniformType::FLOAT2: return "float2";
        case UniformType::FLOAT3: return "float3";
        case UniformType::FLOAT4: return "float4";
        case UniformType::INT:    return "int";
        case UniformType::INT2:   return "int2";
        case UniformType::INT3:   return "int3";
        case UniformType::INT4:   return "int4";
        case UniformType::UINT:   return "uint";
        case UniformType::UINT2:  return "uint2";
        case UniformType::UINT3:  return "uint3";
        case UniformType::UINT4:  return "uint4";
        case UniformType::MAT3:   return "float3x3";
        case UniformType::MAT4:   return "float4x4";
    }
}

template<>
const char* toString(SamplerType type) {
    switch (type) {
        case SamplerType::SAMPLER_2D: return "sampler2D";
        case SamplerType::SAMPLER_CUBEMAP: return "samplerCubemap";
        case SamplerType::SAMPLER_EXTERNAL: return "samplerExternal";
    }
}

template<>
const char* toString(Precision precision) {
    switch (precision) {
        case Precision::LOW: return "lowp";
        case Precision::MEDIUM: return "mediump";
        case Precision::HIGH: return "highp";
        case Precision::DEFAULT: return "default";
    }
}

template<>
const char* toString(SamplerFormat format) {
    switch (format) {
        case SamplerFormat::INT: return "int";
        case SamplerFormat::UINT: return "uint";
        case SamplerFormat::FLOAT: return "float";
        case SamplerFormat::SHADOW: return "shadow";
    }
}

static std::string arraySizeToString(uint64_t size) {
    if (size > 1) {
        std::string s = "[";
        s += size;
        s += "]";
        return s;
    }
    return "";
}

static void printUint32Chunk(ostream& json, const ChunkContainer& container,
        filamat::ChunkType type, const char* title) {
    uint32_t value;
    if (read(container, type, &value)) {
        json << "'" << title << "': " << value << ",\n";
    }
}

static void printStringChunk(ostream& json, const ChunkContainer& container,
        filamat::ChunkType type, const char* title) {
    CString value;
    if (read(container, type, &value)) {
        json << "'" << title << "': '" << value.c_str() << "',\n";
    }
}

static bool printMaterial(ostream& json, const ChunkContainer& container) {
    printStringChunk(json, container, filamat::MaterialName, "name");
    printUint32Chunk(json, container, filamat::MaterialVersion, "version");
    printUint32Chunk(json, container, filamat::PostProcessVersion, "pp_version");
    json << "'shading': {\n";
    // TODO
    json << "},\n";
    json << "'raster': {\n";
    // TODO
    json << "},\n";
    return true;
}

static bool printParametersInfo(ostream& json, const ChunkContainer& container) {
    // TODO
    return true;
}

static void printShaderInfo(ostream& json, const std::vector<ShaderInfo>& info) {
    for (uint64_t i = 0; i < info.size(); ++i) {
        const auto& item = info[i];
        json
            << "    {"
            << "'shaderModel': '" << toString(item.shaderModel) << "', "
            << "'pipelineStage': '" << toString(item.pipelineStage) << "', "
            << "'variant': " << int(item.variant)  << "}"
            << ((i == info.size() - 1) ? "\n" : ",\n");
    }
}

static bool printGlslInfo(ostream& json, const ChunkContainer& container) {
    std::vector<ShaderInfo> info;
    if (!getGlShaderInfo(container, &info)) {
        return false;
    }
    json << "'opengl': [\n";
    printShaderInfo(json, info);
    json << "],\n";
    return true;
}

static bool printVkInfo(ostream& json, const ChunkContainer& container) {
    std::vector<ShaderInfo> info;
    if (!getVkShaderInfo(container, &info)) {
        return false;
    }
    json << "'vulkan': [\n";
    printShaderInfo(json, info);
    json << "],\n";
    return true;
}

static bool printMetalInfo(ostream& json, const ChunkContainer& container) {
    std::vector<ShaderInfo> info;
    if (!getMetalShaderInfo(container, &info)) {
        return false;
    }
    json << "'metal': [\n";
    printShaderInfo(json, info);
    json << "],\n";
    return true;
}

bool JsonGenerator::printMaterialInfo(const filaflat::ChunkContainer& container) {
    ostringstream json;
    json << "{\n";
    if (!printMaterial(json, container)) {
        return false;
    }
    if (!printParametersInfo(json, container)) {
        return false;
    }
    if (!printGlslInfo(json, container)) {
        return false;
    }
    if (!printVkInfo(json, container)) {
        return false;
    }
    if (!printMetalInfo(json, container)) {
        return false;
    }

    json << "'required_attributes': [\n";
    // TODO
    json << "]\n";

    json << "}\n";
    mJsonString = CString(json.str().c_str());
    return true;
}

const char* JsonGenerator::getJsonString() const {
    return mJsonString.c_str();
}

size_t JsonGenerator::getJsonSize() const {
    return mJsonString.size();
}

} // namespace matdbg
} // namespace filament
