// Command-line companion: export / import OBJ without the GUI (also used for tests).
#include "../src/objio.h"
#include <cstdio>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // UTF-8 messages
#endif
    if (argc < 3) {
        std::printf("uso:\n  vrtool info ROM\n  vrtool export ROM CIRCUITO(1-5) salida.obj\n"
                    "  vrtool import ROM CIRCUITO(1-5) entrada.obj salida.32x\n  vrtool roundtrip ROM\n");
        return 1;
    }
    std::string cmd = argv[1], err;
    vr::Rom rom;
    if (!rom.load(argv[2], err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    if (cmd == "info") {
        for (auto &t : rom.tracks) {
            size_t v = 0, f = 0;
            for (auto &b : t.blocks) { v += b.second.verts.size(); f += b.second.faces.size(); }
            std::printf("%-26s bloques %3zu  vertices %5zu  caras %5zu\n", t.name.c_str(), t.blocks.size(), v, f);
        }
        return 0;
    }
    if (cmd == "roundtrip") {           // re-encode every block and compare with the ROM bytes
        int bad = 0, n = 0;
        for (auto &t : rom.tracks)
            for (auto &kv : t.blocks) {
                auto enc = vr::encodeBlock(kv.second);
                ++n;
                if (enc.size() != kv.second.size ||
                    std::memcmp(enc.data(), rom.bytes().data() + kv.second.offset, enc.size()) != 0) ++bad;
            }
        std::printf("%d bloques, %d distintos al recodificar\n", n, bad);
        return bad ? 2 : 0;
    }
    int ti = argc > 3 ? std::atoi(argv[3]) - 1 : -1;
    if (ti < 0 || ti >= int(rom.tracks.size())) { std::fprintf(stderr, "circuito invalido\n"); return 1; }
    if (cmd == "export" && argc > 4) {
        if (!vr::exportObj(rom, rom.tracks[ti], argv[4], err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        return 0;
    }
    if (cmd == "import" && argc > 5) {
        vr::ImportReport rep;
        if (!vr::importObj(rom, rom.tracks[ti], argv[4], rep, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        std::printf("%s%d bloques modificados\n", rep.log.c_str(), rep.blocksChanged);
        if (!rom.save(argv[5], err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
        return 0;
    }
    std::fprintf(stderr, "orden desconocida\n");
    return 1;
}
