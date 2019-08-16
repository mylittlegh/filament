#include <matdbg/MaterialDebugger.h>

#include <utils/Log.h>

#include <CivetServer.h>

#include <utils/Hash.h>

#include <string>

#include "ShaderExtracter.h"
#include "ShaderInfo.h"
#include "JsonGenerator.h"

#include "matdbg_resources.h"

using namespace utils;

static const StaticString kSuccessHeader =
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
        "Connection: close\r\n\r\n";

namespace filament {
namespace matdbg {

class FileRequestHandler : public CivetHandler {
public:
    FileRequestHandler(MaterialDebugger* debugger) : mDebugger(debugger) {}
    bool handleGet(CivetServer *server, struct mg_connection *conn) {
        const struct mg_request_info* request = mg_get_request_info(conn);
        std::string uri(request->request_uri);
        if (uri == "/" || uri == "/index.html") {
            mg_printf(conn, kSuccessHeader.c_str(), "text/html");
            mg_write(conn, mDebugger->mHtml.c_str(), mDebugger->mHtml.size());
            return true;
        }
        if (uri == "/style.css") {
            mg_printf(conn, kSuccessHeader.c_str(), "text/css");
            mg_write(conn, mDebugger->mCss.c_str(), mDebugger->mCss.size());
            return true;
        }
        if (uri == "/script.js") {
            mg_printf(conn, kSuccessHeader.c_str(), "text/javascript");
            mg_write(conn, mDebugger->mJavascript.c_str(), mDebugger->mJavascript.size());
            return true;
        }
        slog.e << "MaterialDebugger: bad request at line " <<  __LINE__ << ": " << uri << io::endl;
        return false;
    }
private:
    MaterialDebugger* mDebugger;
};

// Handles the following REST requests, where {id} is an 8-digit hex string.
//
//    GET /api/material?matid={id}
//    GET /api/shader?matid={id}&type=[glsl|spirv]&[glindex|vkindex|metalindex]={index}
//
// Question: should type also offer "msl" ??
//
class RestRequestHandler : public CivetHandler {
public:
    RestRequestHandler(MaterialDebugger* debugger) : mDebugger(debugger) {}

    bool handleGet(CivetServer *server, struct mg_connection *conn) {
        const struct mg_request_info* request = mg_get_request_info(conn);
        std::string uri(request->local_uri);

        const auto printError = [request](int line) {
            slog.e << "MaterialDebugger: bad REST request at line " <<  line << ": " <<
                    request->query_string << io::endl;
        };

        const size_t qlength = strlen(request->query_string);
        char matid[9] = {};
        if (mg_get_var(request->query_string, qlength, "matid", matid, sizeof(matid)) < 0) {
            printError(__LINE__);
            return false;
        }
        uint32_t id = std::stoul(matid, nullptr, 16);

        if (uri == "/api/material") {
            if (mDebugger->mMaterialPackages.count(id) == 0) {
                printError(__LINE__);
                return false;
            }
            JsonGenerator generator;
            if (!generator.printMaterialInfo(*mDebugger->mMaterialPackages.at(id))) {
                printError(__LINE__);
                return false;
            }
            mg_printf(conn, kSuccessHeader.c_str(), "application/json");
            mg_write(conn, generator.getJsonString(), generator.getJsonSize());
            return true;
        }

        char type[5] = {};
        if (mg_get_var(request->query_string, qlength, "type", type, sizeof(type)) < 0) {
            printError(__LINE__);
            return false;
        }

        char glindex[4] = {};
        char vkindex[4] = {};
        char metalindex[4] = {};
        mg_get_var(request->query_string, qlength, "glindex", glindex, sizeof(glindex));
        mg_get_var(request->query_string, qlength, "vkindex", vkindex, sizeof(vkindex));
        mg_get_var(request->query_string, qlength, "metalindex", metalindex, sizeof(metalindex));

        if (!glindex[0] && !vkindex[0] && !metalindex[0]) {
            printError(__LINE__);
            return false;
        }

        if (uri != "/api/shader") {
            printError(__LINE__);
            return false;
        }

        if (glindex[0]) {
            auto chunk = mDebugger->mMaterialPackages.at(id);
            ShaderExtracter extractor(Backend::OPENGL, chunk->getData(), chunk->getSize());
            if (!extractor.parse() ||
                    (!extractor.isShadingMaterial() && !extractor.isPostProcessMaterial())) {
                return false;
            }

            filaflat::ShaderBuilder builder;
            std::vector<ShaderInfo> info;
            if (!getGlShaderInfo(*chunk, &info)) {
                printError(__LINE__);
                return false;
            }

            const int shaderIndex = stoi(glindex);
            if (shaderIndex >= info.size()) {
                printError(__LINE__);
                return false;
            }

            const auto& item = info[shaderIndex];
            extractor.getShader(item.shaderModel, item.variant, item.pipelineStage, builder);

            mg_printf(conn, kSuccessHeader.c_str(), "application/txt");
            mg_write(conn, builder.data(), builder.size());
            return true;
        }

        if (vkindex[0]) {
            auto chunk = mDebugger->mMaterialPackages.at(id);
            ShaderExtracter extractor(Backend::VULKAN, chunk->getData(), chunk->getSize());
            if (!extractor.parse() ||
                    (!extractor.isShadingMaterial() && !extractor.isPostProcessMaterial())) {
                return false;
            }

            filaflat::ShaderBuilder builder;
            std::vector<ShaderInfo> info;
            if (!getVkShaderInfo(*chunk, &info)) {
                printError(__LINE__);
                return false;
            }

            const int shaderIndex = stoi(glindex);
            if (shaderIndex >= info.size()) {
                printError(__LINE__);
                return false;
            }

            const auto& item = info[shaderIndex];
            extractor.getShader(item.shaderModel, item.variant, item.pipelineStage, builder);

            // TODO: transpile or disassemble, depending on "type"

            mg_printf(conn, kSuccessHeader.c_str(), "application/bin");
            mg_write(conn, builder.data(), builder.size());
            return true;
        }

        if (metalindex[0]) {
            auto chunk = mDebugger->mMaterialPackages.at(id);
            ShaderExtracter extractor(Backend::METAL, chunk->getData(), chunk->getSize());
            if (!extractor.parse() ||
                    (!extractor.isShadingMaterial() && !extractor.isPostProcessMaterial())) {
                return false;
            }

            filaflat::ShaderBuilder builder;
            std::vector<ShaderInfo> info;
            if (!getMetalShaderInfo(*chunk, &info)) {
                printError(__LINE__);
                return false;
            }

            const int shaderIndex = stoi(metalindex);
            if (shaderIndex >= info.size()) {
                printError(__LINE__);
                return false;
            }

            const auto& item = info[shaderIndex];
            extractor.getShader(item.shaderModel, item.variant, item.pipelineStage, builder);

            mg_printf(conn, kSuccessHeader.c_str(), "application/txt");
            mg_write(conn, builder.data(), builder.size());
            return true;
        }

        printError(__LINE__);
        return false;
    }

private:
    MaterialDebugger* mDebugger;
};

MaterialDebugger::MaterialDebugger(ServerMode mode, int port) : mServerMode(mode) {
    mHtml = CString((const char*) MATDBG_RESOURCES_INDEX_DATA, MATDBG_RESOURCES_INDEX_SIZE);
    mJavascript = CString((const char*) MATDBG_RESOURCES_SCRIPT_DATA, MATDBG_RESOURCES_SCRIPT_SIZE);
    mCss = CString((const char*) MATDBG_RESOURCES_STYLE_DATA, MATDBG_RESOURCES_STYLE_SIZE);

    const char* kServerOptions[] = { "listening_ports", "8080", nullptr };
    std::string portString = std::to_string(port);
    kServerOptions[1] = portString.c_str();

    mServer = new CivetServer(kServerOptions);
    mFileHandler = new FileRequestHandler(this);
    mRestHandler = new RestRequestHandler(this);

    mServer->addHandler("/api", mRestHandler);
    mServer->addHandler("", mFileHandler);

    slog.i << "Material debugger listening at http://localhost:" << port << io::endl;
}

MaterialDebugger::~MaterialDebugger() {
    for (auto pair : mMaterialPackages) {
        delete pair.second;
    }
    delete mFileHandler;
    delete mRestHandler;
    delete mServer;
}

MaterialId MaterialDebugger::addMaterialPackage(const void* data, size_t size) {
    const uint32_t seed = 42;
    auto words = (const uint32_t*) data;
    MaterialId id = utils::hash::murmur3(words, size / 4, seed);

printf("prideout observed material load: %8.8x\n", id);

    // TODO: send a WebSockets ping to client

    filaflat::ChunkContainer* package = new filaflat::ChunkContainer(data, size);
    if (!package->parse()) {
        slog.e << "MaterialDebugger: unable to parse material package." << io::endl;
        return 0;
    }

    mMaterialPackages.emplace(id, package);
    return id;
}

bool MaterialDebugger::getEditedMaterialPackage(MaterialId id, const void** data, size_t* size) {
    if (mMaterialPackages.count(id) == 0) {
        return false;
    }

    // TODO: send a WebSockets query to client

    return true;
}

} // namespace matdbg
} // namespace filament
