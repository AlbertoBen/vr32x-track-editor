# Formato de los circuitos (Virtua Racing Deluxe, 32X, USA)

Direcciones en **offset de fichero** de la ROM salvo que se diga otra cosa. Los datos son big-endian.

## Rejilla de cada circuito

Cada circuito tiene una tabla de **32 × 32 punteros de 32 bits** (4 KB). Cada puntero es una dirección SH2 de la ROM con caché desactivada (`0x22xxxxxx` → offset `& 0x3FFFFF`) o el valor `0x2207FFFE` (celda vacía).

| Circuito | Rejilla | Bloques | Paleta |
|---|---|---|---|
| 1 (Big Forest) | `0x15C000` | `0x0A0000`–`0x0BBA7C` | 0 |
| 2 | `0x15D000` | `0x0C0000`–`0x0DC09C` | 1 |
| 3 | `0x15E000` | `0x200000`–`0x21D72E` | 2 |
| 4 | `0x15F000` | `0x220000`–`0x23957C` | 3 |
| 5 | `0x161000` | `0x240000`–`0x25A5DC` | 4 |

Las rejillas están en el banco 1 del 68000 (`0x95C000`… con el banco apuntando a `0x100000`). La tabla de punteros a rejillas está en `0x007248`, indexada por `$C8A0`.

**Celda de una posición del mundo** (código 68K `object_geometry_visibility_collect`, `$00734E`), con X y Z en unidades del mundo (= coordenada de vértice / 2):

```
columna = (X/16 + 0x400) >> 6
fila    = (0x400 - Z/16) >> 6        (Z crece hacia el norte, la fila hacia el sur)
```

Cada frame el 68000 envía al SH2 la lista de bloques de la celda de la cámara y de sus vecinas (tabla de desplazamientos por dirección en `$89A5D2` / `$89A0D4`). El maestro SH2 los ordena por su primera palabra (`0x0600115C`) y el esclavo los dibuja con el renderizador en memoria interna (`0x0600254C` → `C0000000`).

## Bloque

```
+0   u16  número de vértices N
+2   u16  (desconocido; el renderizador lo salta; se conserva)
+4   N × (s16 x, s16 y, s16 z)          coordenadas absolutas (2 × mundo), Y hacia arriba
     caras...
     u16  0x0C00                        fin
```

## Caras

```
u16 cabecera   byte alto & 0x0E = tipo (0, 2, 4, 6; 0x0C = fin)
               bit 0 = triángulo, bit 5 = lleva normal (3 × s16 al final)
u16 color      byte alto = índice de paleta, bit 0 = triángulo
índices        cada uno = vértice × 16 (tamaño del vértice transformado)
```

El renderizador mantiene 4 huecos de vértice (s0..s3). Cada tipo reutiliza dos vértices de la cara anterior:

| Tipo | s0 | s1 | Índices que se leen |
|---|---|---|---|
| 0 | nuevo | nuevo | s0, s1, s2 (+ s3 si es cuadrilátero) |
| 2 | se mantiene | s3 anterior | s2 (+ s3) |
| 4 | s3 anterior | s2 anterior | s2 (+ s3) |
| 6 | s2 anterior | se mantiene | s2 (+ s3) |

En un triángulo s3 = s2. El codificador del editor usa el tipo más corto posible, así que un bloque sin cambios ocupa lo mismo (o menos) que el original.

## Paletas

Seis paletas de 256 colores (BGR555) seguidas desde `0x3A238`, cada 0x200 bytes. La del circuito *k* es la *k*. Los colores `0x2F`–`0x55` se cambian en tiempo de ejecución (marcador).

## Comprobado

- Los 1.096 bloques de los cinco circuitos se leen sin errores y se recodifican con el mismo contenido.
- Exportar e importar sin cambios deja la ROM idéntica byte a byte.
- Un bloque de Big Forest levantado y recoloreado desde el OBJ aparece así en el juego (PicoDrive).
- Orientación: con Y hacia arriba, mirando al oeste el norte queda a la derecha (probado coloreando cuadrantes en el juego).
