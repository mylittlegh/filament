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

#ifndef MATDBG_MATERIALSERVER_H
#define MATDBG_MATERIALSERVER_H

#include <filaflat/ChunkContainer.h>

#include <utils/CString.h>

#include <tsl/robin_map.h>

#include <vector>
#include <cstdint>
#include <stddef.h>

class CivetServer;

namespace filament {
namespace matdbg {

class FileRequestHandler;
class RestRequestHandler;

// The debugger identifies material packages using hashes of their content, rather than GUID's or
// increasing integers.
//
// We use a hash to allow a single debugging session to be used across multiple runs of the app that
// is being debugged while preserving edits. The developer can edit a material in the debugger, then
// keep the debugger open while relaunching (or refreshing) the app.
//
// TODO: Is a 32-bit murmur hash really the right thing to use here? I think I would prefer a
//       git-style SHA1 string, which could be displayed in the UI and basically never have
//       collisions.
using MaterialId = uint32_t;

// The debugger server can be linked into the Filament Engine (allowing live inspection of GLSL /
// SPIRV) or into the matinfo tool (allowing web-based editing of filamat files).
enum ServerMode { ENGINE, STANDALONE };

/**
 * Server-side debugger API.
 *
 * Spins up a web server and receives materials from the Filament C++ engine or from
 * the matinfo command-line tool. Also responds to requests from client-side javascript.
 */
class MaterialDebugger {
public:
    MaterialDebugger(ServerMode mode, int port = 8080);
    ~MaterialDebugger();

    /**
     * Notifies the client debugging session that the given material package is being loaded into
     * the engine and returns a unique identifier for the package, which is actually a hash of its
     * contents.
     */
    MaterialId addMaterialPackage(const void* data, size_t size);

    /**
     * Asks the client debugging session if the given material package has been edited, and if so
     * returns the new contents of the package.
     *
     * Returns false if no material package with the given id is known by the debugging session.
     *
     * If the given material package has not been edited in this session, returns true and the
     * data/size pointers are left unchanged.
     *
     * If the given material package has been edited in this session, returns true and the data/size
     * pointers are set to the edited contents of the package.
     */
    bool getEditedMaterialPackage(MaterialId id, const void** data, size_t* size);

    friend class FileRequestHandler;
    friend class RestRequestHandler;

private:
    const ServerMode mServerMode;
    CivetServer* mServer;
    tsl::robin_map<MaterialId, filaflat::ChunkContainer*> mMaterialPackages;
    utils::CString mHtml;
    utils::CString mJavascript;
    utils::CString mCss;
    FileRequestHandler* mFileHandler;
    RestRequestHandler* mRestHandler;
};

} // namespace matdbg
} // namespace filament

#endif  // MATDBG_MATERIALSERVER_H
