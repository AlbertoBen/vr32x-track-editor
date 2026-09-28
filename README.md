# VR32X Track Editor

Visor y editor de los circuitos de **Virtua Racing Deluxe** (Sega 32X). Funciona con la ROM original (USA) y con las versiones a 30 FPS.

![estado](https://img.shields.io/badge/versi%C3%B3n-0.1-blue)

## Qué hace (v0.1)

- **Abre la ROM** y muestra los cinco circuitos en 3D, con sus colores reales (paleta de cada circuito).
- **Mapa de bloques:** cada circuito es una rejilla de 32 × 32 celdas. Se puede seleccionar cualquier bloque en el mapa o en la vista 3D.
- **Exporta a OBJ + MTL**, el circuito entero o un solo bloque, para editarlo en Blender u otro programa.
- **Importa el OBJ editado** y guarda una ROM nueva: mover vértices, cambiar colores y cambiar caras, siempre que cada bloque quepa en el espacio que ocupaba.
- **`vrtool`**: la misma exportación e importación desde la línea de órdenes.

## Uso

1. Ejecuta `vr32x-track-editor.exe` y abre la ROM (**Archivo › Abrir ROM**, o arrástrala a la ventana).
2. Elige el circuito a la izquierda.
3. **Exportar circuito** → edítalo en Blender (importar como OBJ).
4. **Importar OBJ...** → **Guardar ROM** (se guarda con otro nombre, la original no se toca).

### Controles de la vista 3D

| Acción | Control |
|---|---|
| Girar | botón derecho + arrastrar |
| Desplazar | botón central, o Mayús + botón derecho |
| Acercar / alejar | rueda |
| Moverse | W A S D, Q / E para bajar / subir (Mayús = rápido) |
| Seleccionar bloque | clic |
| Centrar selección / vista superior / todo | F / T / Inicio |

### Reglas del OBJ

- Cada objeto es un bloque y se llama `cell_FILA_COLUMNA_OFFSET`. **No cambies los nombres.**
- El material `cXX` es el color `XX` (hexadecimal) de la paleta del circuito (**Ver › Paleta de colores**).
- Solo triángulos y cuadriláteros. Unidades: 1 unidad OBJ = 256 unidades de la ROM. Ejes: X este, Y arriba, Z sur.
- Un bloque no puede crecer más que su hueco en la ROM (se avisa al importar con los bytes que faltan).
- El juego solo dibuja la celda de la cámara y sus vecinas: la geometría de un bloque debe quedarse cerca de su celda.
- La física y las colisiones usan **otros datos**: cambiar la forma de la carretera no cambia por dónde circula el coche. Es el siguiente paso del proyecto.

## Compilar

**Windows (desde Linux, con llvm-mingw):** `MINGW=/ruta/llvm-mingw ./build_windows.sh` → `build/windows/`

**Windows (Visual Studio o MinGW, con CMake):**
```
cmake -B build -S .
cmake --build build --config Release
```

**Linux (X11):** `./build_linux.sh` → `build/linux/`

Sin dependencias externas: Dear ImGui y stb van incluidos en `third_party/`.

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/rom.*` | Lectura de la ROM: rejillas, bloques, caras, paletas; codificación de bloques |
| `src/objio.*` | Exportación e importación OBJ/MTL |
| `src/renderer.*`, `src/gl.*` | Vista 3D (OpenGL 3.3) y selección con el ratón |
| `src/app.*` | Interfaz (Dear ImGui) |
| `src/platform_win32.cpp`, `src/platform_x11.cpp` | Ventana, entrada y diálogos de cada sistema |
| `tools/vrtool.cpp` | Herramienta de línea de órdenes |
| `docs/FORMAT.md` | Formato de los circuitos en la ROM |

## Próximos pasos

1. Bloques que crecen: mover un bloque editado a espacio libre de la ROM.
2. Datos de física y colisión (superficie y altura) generados a partir de la geometría.
3. Objetos fuera de la rejilla (edificios y decorado que no están en los bloques).
4. Trazada de la IA y puntos de control → circuitos nuevos.

## Créditos

- Formato de los circuitos descifrado sobre el desensamblado [sega-vr-disasm](https://github.com/matiaszanolli/sega-vr-disasm).
- [Dear ImGui](https://github.com/ocornut/imgui) (MIT), [stb_image_write](https://github.com/nothings/stb) (dominio público).
