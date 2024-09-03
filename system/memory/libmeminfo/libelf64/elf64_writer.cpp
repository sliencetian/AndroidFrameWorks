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

#include <libelf64/elf64_writer.h>

#include <libelf64/elf64.h>

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <elf.h>

namespace android {
namespace elf64 {

void Elf64Writer::WriteElf64File(const Elf64Binary& elf64Binary, const std::string& fileName) {
    std::cout << "Writing ELF64 binary to file " << fileName << std::endl;

    Elf64Writer elf64Writer(fileName);
    elf64Writer.WriteHeader(elf64Binary.ehdr);
    elf64Writer.WriteProgramHeaders(elf64Binary.phdrs, elf64Binary.ehdr.e_phoff);
    elf64Writer.WriteSections(elf64Binary.sections, elf64Binary.shdrs);
    elf64Writer.WriteSectionHeaders(elf64Binary.shdrs, elf64Binary.ehdr.e_shoff);
}

Elf64Writer::Elf64Writer(const std::string& fileName) {
    elf64stream.open(fileName.c_str(), std::ofstream::out | std::ofstream::binary);
    if (!elf64stream) {
        std::cerr << "Failed to open the file: " << fileName << std::endl;
        exit(-1);
    }
}

void Elf64Writer::WriteHeader(const Elf64_Ehdr& ehdr) {
    Write((char*)&ehdr, sizeof(ehdr));
}

void Elf64Writer::WriteProgramHeaders(const std::vector<Elf64_Phdr>& phdrs, const Elf64_Off phoff) {
    elf64stream.seekp(phoff);

    for (int i = 0; i < phdrs.size(); i++) {
        Write((char*)&phdrs[i], sizeof(phdrs[i]));
    }
}

void Elf64Writer::WriteSectionHeaders(const std::vector<Elf64_Shdr>& shdrs, const Elf64_Off shoff) {
    elf64stream.seekp(shoff);

    for (int i = 0; i < shdrs.size(); i++) {
        Write((char*)&shdrs[i], sizeof(shdrs[i]));
    }
}

void Elf64Writer::WriteSections(const std::vector<Elf64_Sc>& sections,
                                const std::vector<Elf64_Shdr>& shdrs) {
    for (int i = 0; i < sections.size(); i++) {
        if (shdrs[i].sh_type == SHT_NOBITS) {
            // Skip .bss section because it is empty.
            continue;
        }

        // Move the cursor position to offset provided by the section header.
        elf64stream.seekp(shdrs[i].sh_offset);

        Write(sections[i].data.data(), sections[i].size);
    }
}

void Elf64Writer::Write(const char* const data, const std::streamsize size) {
    elf64stream.write(data, size);
    if (!elf64stream) {
        std::cerr << "Failed to write [" << size << "] bytes" << std::endl;
        exit(-1);
    }
}

}  // namespace elf64
}  // namespace android
