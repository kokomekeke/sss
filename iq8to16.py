#!/usr/bin/env python3

"""
IQ8 (signed int8 I/Q) -> IQ16 (signed int16 / short I/Q) konverter.

Nagy fájlokhoz készült, chunk-alapú feldolgozással.

Használat:

    ./iq8to16.py input_iq8.bin output_iq16.bin

vagy:

    ./iq8to16.py input_iq8.bin output_iq16.bin 256

Formátum:

    Input:
        int8 I, int8 Q, int8 I, int8 Q, ...

    Output:
        int16 I, int16 Q, int16 I, int16 Q, ...

Alapértelmezett skála:

    256

Példák:

    -128 -> -32768
    -127 -> -32512
       -1 ->   -256
        0 ->      0
        1 ->    256
      100 ->  25600
      127 ->  32512
"""

import sys
import os
import numpy as np


# ------------------------------------------------------------
# Beállítások
# ------------------------------------------------------------

DEFAULT_SCALE = 256

# Egy chunk mérete byte-ban.
# 64 MiB memóriaigény szempontjából biztonságos.
CHUNK_SIZE = 64 * 1024 * 1024


def main():

    # --------------------------------------------------------
    # Argumentumok ellenőrzése
    # --------------------------------------------------------

    if len(sys.argv) < 3:
        print(
            f"Használat: {sys.argv[0]} "
            "input_iq8.bin output_iq16.bin [scale]"
        )
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2]

    if len(sys.argv) >= 4:
        try:
            scale = int(sys.argv[3])
        except ValueError:
            print("Hiba: a scale egész szám kell legyen.")
            sys.exit(1)
    else:
        scale = DEFAULT_SCALE

    if scale <= 0:
        print("Hiba: a scale értékének pozitívnak kell lennie.")
        sys.exit(1)

    # --------------------------------------------------------
    # Bemeneti fájl ellenőrzése
    # --------------------------------------------------------

    if not os.path.isfile(infile):
        print(f"Hiba: a bemeneti fájl nem található:")
        print(f"  {infile}")
        sys.exit(1)

    file_size = os.path.getsize(infile)

    if file_size == 0:
        print("Hiba: a bemeneti fájl üres.")
        sys.exit(1)

    # I/Q interleaved adat esetén páros számú byte kell.
    if file_size % 2 != 0:
        print(
            "Hiba: a bemeneti fájl mérete páratlan. "
            "Az I/Q interleaved fájlnak páros számú byte-ot "
            "kell tartalmaznia."
        )
        sys.exit(1)

    # --------------------------------------------------------
    # Információk
    # --------------------------------------------------------

    total_iq_values = file_size
    total_iq_samples = file_size // 2

    output_size = file_size * 2

    print()
    print("==============================================")
    print("       IQ8 -> IQ16 KONVERTER")
    print("==============================================")
    print()
    print(f"Bemeneti fájl : {infile}")
    print(f"Kimeneti fájl : {outfile}")
    print()
    print(f"Input méret   : {file_size / (1024**3):.2f} GiB")
    print(f"Output méret  : {output_size / (1024**3):.2f} GiB")
    print(f"IQ minták     : {total_iq_samples:,}")
    print(f"Skála         : {scale}")
    print(f"Chunk méret   : {CHUNK_SIZE / (1024**2):.0f} MiB")
    print()
    print("Feldolgozás indul...")
    print()

    # --------------------------------------------------------
    # Chunk-alapú feldolgozás
    # --------------------------------------------------------

    processed = 0

    with open(infile, "rb") as fin, open(outfile, "wb") as fout:

        while True:

            # ------------------------------------------------
            # Chunk beolvasása
            # ------------------------------------------------

            raw = fin.read(CHUNK_SIZE)

            if not raw:
                break

            # ------------------------------------------------
            # IQ8 adatok numpy tömbként
            # ------------------------------------------------

            data_i8 = np.frombuffer(
                raw,
                dtype=np.int8
            )

            # ------------------------------------------------
            # I/Q párok ellenőrzése
            # ------------------------------------------------

            if data_i8.size % 2 != 0:

                # Ez csak akkor fordulhat elő, ha a chunk mérete
                # nem páros, vagy hibás adat érkezett.
                print(
                    "Hiba: páratlan számú IQ érték egy chunkban."
                )
                sys.exit(1)

            # ------------------------------------------------
            # IQ8 -> IQ16
            #
            # int32 köztes típust használunk.
            # ------------------------------------------------

            data_i16 = (
                data_i8.astype(np.int32) * scale
            )

            # ------------------------------------------------
            # int16 tartományra korlátozás
            # ------------------------------------------------

            data_i16 = np.clip(
                data_i16,
                -32768,
                32767
            )

            # ------------------------------------------------
            # int16
            # ------------------------------------------------

            data_i16 = data_i16.astype(
                np.int16
            )

            # ------------------------------------------------
            # Kiírás
            # ------------------------------------------------

            fout.write(
                data_i16.tobytes()
            )

            # ------------------------------------------------
            # Progress
            # ------------------------------------------------

            processed += len(raw)

            percent = (
                processed / file_size
            ) * 100.0

            print(
                f"\rFeldolgozás: {percent:6.2f}%",
                end="",
                flush=True
            )

    # --------------------------------------------------------
    # Ellenőrzés
    # --------------------------------------------------------

    if os.path.isfile(outfile):
        actual_output_size = os.path.getsize(outfile)
    else:
        actual_output_size = 0

    print()
    print()
    print("==============================================")
    print("Konverzió kész.")
    print("==============================================")
    print()
    print(f"Bemenet : {file_size / (1024**3):.2f} GiB")
    print(f"Kimenet : {actual_output_size / (1024**3):.2f} GiB")
    print()

    if actual_output_size == output_size:
        print("Méret ellenőrzés: OK")
    else:
        print("FIGYELEM: a kimeneti fájl mérete nem megfelelő!")
        print(f"Elvárt : {output_size}")
        print(f"Kapott : {actual_output_size}")

    print()


if __name__ == "__main__":
    main()
