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

#ifndef MATDBG_JSONGENERATOR_H
#define MATDBG_JSONGENERATOR_H

#include <CivetServer.h>

#include <filaflat/ChunkContainer.h>

#include <utils/CString.h>

namespace filament {
namespace matdbg {

class JsonGenerator {
public:
    bool printMaterialInfo(const filaflat::ChunkContainer& package);
    const char* getJsonString() const;
    size_t getJsonSize() const;
private:
    mg_connection* mConnection;
    utils::CString mJsonString;
};

} // namespace matdbg
} // namespace filament

#endif  // MATDBG_MATERIALSERVER_H
