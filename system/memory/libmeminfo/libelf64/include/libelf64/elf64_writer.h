/*
 * Copyright (C) 2024 The Android Open Source Project
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

#pragma once

#include <libelf64/elf64.h>

#include <stdint.h>
#include <fstream>
#include <string>
#include <vector>

#include <elf.h>

namespace android {
namespace elf64 {

// Class to write elf64 binaries to files. It provides methods
// to write the different parts of the efl64 binary:
//
// - Executable Header (Elf64_Ehdr)
// - Program Headers (Elf64_Phdr)
// - Section Headers (Elf64_Shdr)
// - Sections (content)
//
// The basic usage of the library is:
//
//       android::elf64::Elf64Binary elf64Binary;
//       // Populate elf64Binary
//       elf64Binary.ehdr.e_phoff = 0xBEEFFADE
//       std::string fileName("new_binary.so");
//       android::elf64::Elf64Writer::WriteElfFile(elf64Binary, fileName);
//
// If it is necessary to have more control about the different parts
// that need to be written or omitted, we can use:
//
//       android::elf64::Elf64Binary elf64Binary;
//       // Populate elf64Binary
//
//       std::string fileName("new_binary.so");
//       Elf64Writer elf64Writer(fileName);
//
//       elf64Writer.WriteHeader(elf64Binary.ehdr);
//       elf64Writer.WriteProgramHeaders(elf64Binary.phdrs, 0xBEEF);
//       elf64Writer.WriteSectionHeaders(elf64Binary.shdrs, 0xFADE);
//       elf64Writer.WriteSections(elf64Binary.sections, elf64Binary.shdrs);
//
class Elf64Writer {
  public:
    Elf64Writer(const std::string& fileName);

    void WriteHeader(const Elf64_Ehdr& ehdr);
    void WriteProgramHeaders(const std::vector<Elf64_Phdr>& phdrs, const Elf64_Off phoff);
    void WriteSectionHeaders(const std::vector<Elf64_Shdr>& shdrs, const Elf64_Off shoff);
    void WriteSections(const std::vector<Elf64_Sc>& sections, const std::vector<Elf64_Shdr>& shdrs);

    static void WriteElf64File(const Elf64Binary& elf64Binary, const std::string& fileName);

  private:
    std::ofstream elf64stream;
    void Write(const char* const data, const std::streamsize size);
};

}  // namespace elf64
}  // namespace android
