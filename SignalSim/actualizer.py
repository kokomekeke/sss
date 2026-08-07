#!/usr/bin/env python3
"""
download_and_actualize.py
Letölti a legújabb elérhető IGS Mixed RINEX fájlt és átdatálja
a JSON-ban megadott dátumra.
toe/toc/week/tx_time értékek abszolút GPS időben frissítve.
"""

import json
import re
import sys
import argparse
import urllib.request
import gzip
import os
from datetime import datetime, timedelta

def parse_args():
    parser = argparse.ArgumentParser(description='Download + Actualize RINEX')
    parser.add_argument('-c', '--config', required=True, help='JSON config file')
    parser.add_argument('-o', '--output', required=True, help='Output RINEX file')
    parser.add_argument('-i', '--input', required=False, help='Input RINEX file (skip download)')
    return parser.parse_args()

def read_json_time(config_path):
    with open(config_path, 'r') as f:
        content = f.read()
    content = re.sub(r':\s*0+(\d+)', r': \1', content)
    config = json.loads(content)
    t = config['time']
    return datetime(
        year   = t['year'],
        month  = t['month'],
        day    = t['day'],
        hour   = t['hour'],
        minute = t['minute'],
        second = int(t.get('second', 0))
    )

def download_rinex(target_date, tmp_path):
    for days_back in range(0, 6):
        d = target_date - timedelta(days=days_back)
        doy  = d.timetuple().tm_yday
        year = d.year
        url  = (f"https://igs.bkg.bund.de/root_ftp/IGS/BRDC/{year}/{doy:03d}/"
                f"BRDM00DLR_S_{year}{doy:03d}0000_01D_MN.rnx.gz")
        print(f"[INFO]\tTrying: {url}")
        try:
            tmp_gz = tmp_path + ".gz"
            urllib.request.urlretrieve(url, tmp_gz)
            with gzip.open(tmp_gz, 'rb') as gz_in:
                with open(tmp_path, 'wb') as f_out:
                    f_out.write(gz_in.read())
            os.remove(tmp_gz)
            print(f"[INFO]\tDownloaded RINEX for {d.strftime('%Y-%m-%d')} (DOY {doy})")
            return d
        except Exception as e:
            print(f"[WARN]\tFailed ({e}), trying previous day...")
    raise RuntimeError("Could not download RINEX from BKG after 6 attempts")

# ─── GPS idő számítások ───────────────────────────────────────────────────────

GPS_EPOCH = datetime(1980, 1, 6)
BDS_EPOCH = datetime(2006, 1, 1)

def dt_to_gps_total_sec(dt):
    """UTC datetime -> GPS teljes másodperc (GPS epoch óta)"""
    return int((dt - GPS_EPOCH).total_seconds())

def gps_total_sec_to_week_and_tow(total_sec):
    """GPS teljes másodperc -> (hétszám, tow másodperc a héten belül)"""
    week = total_sec // 604800
    tow  = total_sec %  604800
    return week, tow

def dt_to_gps_week_tow(dt):
    return gps_total_sec_to_week_and_tow(dt_to_gps_total_sec(dt))

def dt_to_bds_week_tow(dt):
    """UTC datetime -> BDS hétszám és tow"""
    total_sec = int((dt - BDS_EPOCH).total_seconds())
    week = total_sec // 604800
    tow  = total_sec %  604800
    return week, tow

# ─── RINEX sor segédfüggvények ───────────────────────────────────────────────

def parse_eph_line(line):
    try:
        year   = int(line[4:8])
        month  = int(line[9:11])
        day    = int(line[12:14])
        hour   = int(line[15:17])
        minute = int(line[18:20])
        second = int(line[21:23])
        return datetime(year, month, day, hour, minute, second)
    except:
        return None

def format_eph_line(line, new_dt):
    date_str = (f"{new_dt.year:4d} {new_dt.month:2d} {new_dt.day:2d} "
                f"{new_dt.hour:2d} {new_dt.minute:2d} {new_dt.second:2d}")
    return line[:4] + date_str + line[23:]

def format_rinex_float(value):
    """19 karakteres RINEX tudományos jelölés"""
    s = f"{value:19.12e}".upper()
    s = re.sub(r'E\+0*(\d+)', lambda m: f"E+{m.group(1)}", s)
    s = re.sub(r'E-0*(\d+)',  lambda m: f"E-{m.group(1)}", s)
    return s

def read_rinex_fields(line):
    fields = []
    for i in range(4):
        start = 4 + i * 19
        end   = start + 19
        fields.append(line[start:end] if end <= len(line) else ' ' * 19)
    return fields

def write_rinex_fields(line, fields):
    return line[:4] + ''.join(fields) + '\n'

def try_float(s):
    try:
        return float(s.replace('D', 'E').replace('d', 'e'))
    except:
        return None

# ─── Toe/week/tx_time frissítők ──────────────────────────────────────────────

def update_toe_absolute(line, field_index, orig_eph_dt, sim_start, system):
    fields = read_rinex_fields(line)
    toe_val = try_float(fields[field_index])
    if toe_val is None:
        return line

    if system in ('G', 'E', 'J'):
        orig_week, _ = dt_to_gps_week_tow(orig_eph_dt)
        orig_toe_total = orig_week * 604800 + int(toe_val)
        orig_toc_total = dt_to_gps_total_sec(orig_eph_dt)
        if abs(orig_toe_total - orig_toc_total) > 302400:
            if orig_toe_total < orig_toc_total:
                orig_toe_total += 604800
            else:
                orig_toe_total -= 604800

        sim_total   = dt_to_gps_total_sec(sim_start)
        orig_total  = dt_to_gps_total_sec(orig_eph_dt)
        time_offset = sim_total - orig_total

        new_toe_total       = orig_toe_total + time_offset
        _, new_tow          = gps_total_sec_to_week_and_tow(new_toe_total)
        fields[field_index] = format_rinex_float(float(new_tow))
        return write_rinex_fields(line, fields)

    elif system == 'C':
        orig_week_bds, _ = dt_to_bds_week_tow(orig_eph_dt)
        orig_toe_total   = orig_week_bds * 604800 + int(toe_val)
        orig_toc_total   = int((orig_eph_dt - BDS_EPOCH).total_seconds())
        if abs(orig_toe_total - orig_toc_total) > 302400:
            if orig_toe_total < orig_toc_total:
                orig_toe_total += 604800
            else:
                orig_toe_total -= 604800

        sim_total   = int((sim_start - BDS_EPOCH).total_seconds())
        orig_total  = int((orig_eph_dt - BDS_EPOCH).total_seconds())
        time_offset = sim_total - orig_total

        new_toe_total       = orig_toe_total + time_offset
        _, new_tow          = gps_total_sec_to_week_and_tow(new_toe_total)
        fields[field_index] = format_rinex_float(float(new_tow))
        return write_rinex_fields(line, fields)

    return line

def update_week_absolute(line, field_index, orig_eph_dt, sim_start, system):
    """
    GPS/BDS week mező frissítése.

    Nem az eredeti week mezőből indulunk ki, hanem a szimulációs
    időpontból számítjuk ki az új week számot. Így akkor is helyes
    lesz, ha az eredeti RINEX-ben rollover vagy hibás week szerepel.
    """

    fields = read_rinex_fields(line)

    if system in ('G', 'E', 'J'):
        sim_week, sim_tow = dt_to_gps_week_tow(sim_start)
        new_week = sim_week

    elif system == 'C':
        sim_week, sim_tow = dt_to_bds_week_tow(sim_start)
        new_week = sim_week

    else:
        return line

    print(
        f"DEBUG WEEK UPDATE: "
        f"sys={system} "
        f"orig_dt={orig_eph_dt} "
        f"sim_dt={sim_start} "
        f"new_week={new_week}"
    )

    fields[field_index] = format_rinex_float(float(new_week))

    return write_rinex_fields(line, fields)

def update_tx_time_absolute(line, field_index, orig_eph_dt, sim_start, system):
    """
    Transmission time of message frissítése GPS TOW másodpercben.

    A tx_time a hétszámon belüli TOW érték — ugyanolyan logikával
    frissítendő, mint a toe: az eredeti toc-hoz viszonyított eltolás
    alapján az új sim_start-hoz igazítjuk, héthatárt kezelve.

    GPS és Galileo esetén egyaránt GPS TOW-ban tárolt.
    """
    if system not in ('G', 'E', 'J'):
        return line

    fields  = read_rinex_fields(line)
    tx_val  = try_float(fields[field_index])
    if tx_val is None:
        return line

    # tx_time a toc hetén belüli TOW — abszolút GPS másodpercre hozzuk
    orig_week, _ = dt_to_gps_week_tow(orig_eph_dt)
    orig_tx_total = orig_week * 604800 + int(tx_val)

    # héthatár-korrekció: tx_time nem lehet 302400 mp-nél távolabb toc-tól
    orig_toc_total = dt_to_gps_total_sec(orig_eph_dt)
    if abs(orig_tx_total - orig_toc_total) > 302400:
        if orig_tx_total < orig_toc_total:
            orig_tx_total += 604800
        else:
            orig_tx_total -= 604800

    # ugyanannyi eltolás, mint toc esetén
    time_offset   = dt_to_gps_total_sec(sim_start) - orig_toc_total
    new_tx_total  = orig_tx_total + time_offset
    _, new_tow    = gps_total_sec_to_week_and_tow(new_tx_total)

    fields[field_index] = format_rinex_float(float(new_tow))
    return write_rinex_fields(line, fields)

# ─── RINEX aktualizálás ───────────────────────────────────────────────────────

def actualize_rinex(input_path, output_path, sim_start):
    in_header       = True
    rinex4_eph_next = False
    first_eph_time  = None
    processed       = 0
    output_lines    = []
    data_line_count = 0
    current_system  = None
    current_eph_dt  = None

    with open(input_path, 'r', errors='replace') as fin:
        for line in fin:

            # fejléc
            if in_header:
                output_lines.append(line)
                if 'END OF HEADER' in line:
                    in_header = False
                continue

            # RINEX4 blokk jelző
            if line[0] == '>':
                output_lines.append(line)
                rinex4_eph_next = (len(line) > 4 and
                                   line[2] == 'E' and line[3] == 'P' and line[4] == 'H')
                data_line_count = 0
                continue

            # ephemeris fejlécsor azonosítása
            is_eph_line = False
            if rinex4_eph_next:
                is_eph_line     = True
                rinex4_eph_next = False
                data_line_count = 0
            elif len(line) > 0 and line[0] in ('G', 'C', 'E', 'R', 'J'):
                is_eph_line     = True
                data_line_count = 0
            else:
                data_line_count += 1

            # ephemeris fejlécsor feldolgozása
            if is_eph_line:
                current_system = line[0] if len(line) > 0 else None
                orig_dt = parse_eph_line(line)
                if orig_dt:
                    current_eph_dt = orig_dt

                    if first_eph_time is None:
                        first_eph_time = orig_dt
                        orig_w, orig_tow = dt_to_gps_week_tow(orig_dt)
                        sim_w,  sim_tow  = dt_to_gps_week_tow(sim_start)
                        print(f"[INFO]\tFirst RINEX time  : {orig_dt.strftime('%Y-%m-%d %H:%M:%S')}")
                        print(f"[INFO]\tSimulation time   : {sim_start.strftime('%Y-%m-%d %H:%M:%S')}")
                        print(f"[INFO]\tGPS week offset   : {sim_w - orig_w} ({orig_w} -> {sim_w})")
                        print(f"[INFO]\tGPS tow offset    : {sim_tow - orig_tow} ({orig_tow} -> {sim_tow})")

                    offset = orig_dt - first_eph_time
                    new_dt = sim_start + offset
                    line   = format_eph_line(line, new_dt)
                    processed += 1

            # adatsorok frissítése
            elif (len(line) > 4 and line[0] == ' '
                  and current_system is not None
                  and current_eph_dt is not None):

                if current_system in ('G', 'E', 'J'):
                    if data_line_count == 3:
                        line = update_toe_absolute(line, 0, current_eph_dt, sim_start, current_system)
                    elif data_line_count == 5:  # ← volt: 6
                        line = update_week_absolute(line, 2, current_eph_dt, sim_start, current_system)
                    elif data_line_count == 7:  # ← volt: 8
                        line = update_tx_time_absolute(line, 0, current_eph_dt, sim_start, current_system)

                elif current_system == 'C':
                    if data_line_count == 3:
                        line = update_toe_absolute(line, 0, current_eph_dt, sim_start, current_system)
                    elif data_line_count == 5:  # ← volt: 6
                        line = update_week_absolute(line, 2, current_eph_dt, sim_start, current_system)

                # GLONASS: fejlécsor dátuma elegendő, tb a C++ kódban számolódik

            output_lines.append(line)

    with open(output_path, 'w') as fout:
        fout.writelines(output_lines)

    print(f"[INFO]\tProcessed {processed} ephemeris records")
    print(f"[INFO]\tOutput written to: {output_path}")

# ─── Main ─────────────────────────────────────────────────────────────────────
def main():
    args = parse_args()

    print("=" * 80)
    print("              RINEX DOWNLOAD + ACTUALIZE")
    print("=" * 80)

    sim_start = read_json_time(args.config)
    print(f"[INFO]\tTarget simulation time: {sim_start.strftime('%Y-%m-%d %H:%M:%S')}")

    tmp_rinex = "/tmp/downloaded_rinex.rnx"

    if args.input:
        print(f"[INFO]\tUsing local input file: {args.input}")
        tmp_rinex = args.input
    else:
        download_rinex(datetime.utcnow(), tmp_rinex)

    actualize_rinex(tmp_rinex, args.output, sim_start)

    print("[INFO]\tDone.")

if __name__ == '__main__':
    main()