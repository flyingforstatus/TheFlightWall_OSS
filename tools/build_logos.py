#!/usr/bin/env python3
"""
build_logos.py — FlightWall local airline logo builder
=======================================================
Downloads PNG logos for the top commercial airlines, converts them to
24×24 RGB565 binary blobs, and writes them to firmware/data/logos/{ICAO}.bin
ready for upload to the ESP32 via LittleFS.

Usage:
    pip install Pillow requests
    python tools/build_logos.py [--width W] [--height H] [--out OUT_DIR]

Logo source:
    Uses the free Airhex CDN (no API key required for small-resolution PNGs).
    URL: https://content.airhex.com/content/logos/airlines_{IATA}_{W}_{H}_r.png
    Each airline entry below maps ICAO -> IATA so both lookup systems work.

Transparency:
    Transparent and background pixels are encoded as 0xF81F (pure magenta in
    RGB565). NeoMatrixDisplay skips these pixels when drawing, letting the
    black LED background show through.

Output:
    firmware/data/logos/{ICAO_UPPERCASE}.bin
    Each file: W * H * 2 bytes, little-endian uint16_t RGB565, row-major.

Customisation:
    Add airlines to AIRLINE_LIST below as ("ICAO", "IATA", "Human name").
    IATA is used for the logo fetch URL; ICAO is the filename and lookup key.
"""

import argparse
import io
import struct
import sys
import time
from pathlib import Path

try:
    import requests
    from PIL import Image
except ImportError:
    print("ERROR: Missing dependencies. Run: pip install Pillow requests")
    sys.exit(1)

# ─── Configurable defaults ────────────────────────────────────────────────────
DEFAULT_WIDTH  = 32
DEFAULT_HEIGHT = 32
SCRIPT_DIR     = Path(__file__).parent
DEFAULT_OUT    = SCRIPT_DIR.parent / "firmware" / "data" / "logos"

# Transparent colour: pure magenta in RGB565
TRANSPARENT_RGB565 = 0xF81F

# Delay between requests to be a polite CDN user
REQUEST_DELAY_S = 0.5

# ─── Airline list ─────────────────────────────────────────────────────────────
# Format: ("ICAO", "IATA", "Display name for logging")
# ICAO is the lookup key on the ESP32; IATA is used in the logo URL.
# Ordered roughly by global passenger volume so partial runs cover the
# most common airlines first.
AIRLINE_LIST = [
    # ── United States ─────────────────────────────────────────────────────────
    ("AAL", "AA", "American Airlines"),
    ("DAL", "DL", "Delta Air Lines"),
    ("UAL", "UA", "United Airlines"),
    ("SWA", "WN", "Southwest Airlines"),
    ("ASA", "AS", "Alaska Airlines"),
    ("JBU", "B6", "JetBlue Airways"),
    ("SKW", "OO", "SkyWest Airlines"),
    ("FFT", "F9", "Frontier Airlines"),
    ("NKS", "NK", "Spirit Airlines"),
    ("HAL", "HA", "Hawaiian Airlines"),
    ("SUN", "SY", "Sun Country Airlines"),
    ("GTI", "GS", "Atlas Air"),
    ("ABX", "GB", "ABX Air"),
    ("FDX", "FX", "FedEx Express"),
    ("UPS", "5X", "UPS Airlines"),

    # ── Canada ────────────────────────────────────────────────────────────────
    ("ACA", "AC", "Air Canada"),
    ("WJA", "WS", "WestJet"),
    ("TTS", "TS", "Air Transat"),

    # ── Europe ────────────────────────────────────────────────────────────────
    ("RYR", "FR", "Ryanair"),
    ("EZY", "U2", "easyJet"),
    ("DLH", "LH", "Lufthansa"),
    ("BAW", "BA", "British Airways"),
    ("AFR", "AF", "Air France"),
    ("KLM", "KL", "KLM Royal Dutch"),
    ("IBE", "IB", "Iberia"),
    ("VLG", "VY", "Vueling"),
    ("NAX", "DY", "Norwegian"),
    ("SAS", "SK", "Scandinavian Airlines"),
    ("AZA", "AZ", "ITA Airways"),
    ("TAP", "TP", "TAP Air Portugal"),
    ("AUA", "OS", "Austrian Airlines"),
    ("SWR", "LX", "Swiss International"),
    ("BEL", "SN", "Brussels Airlines"),
    ("THY", "TK", "Turkish Airlines"),
    ("WZZ", "W6", "Wizz Air"),
    ("TOM", "BY", "TUI Airways"),
    ("AEE", "A3", "Aegean Airlines"),
    ("LOT", "LO", "LOT Polish Airlines"),
    ("CSA", "OK", "Czech Airlines"),
    ("FIN", "AY", "Finnair"),
    ("ICE", "FI", "Icelandair"),
    ("UAE", "EK", "Emirates"),
    ("ETD", "EY", "Etihad Airways"),
    ("QTR", "QR", "Qatar Airways"),

    # ── Asia-Pacific ─────────────────────────────────────────────────────────
    ("CCA", "CA", "Air China"),
    ("CSN", "CZ", "China Southern"),
    ("CES", "MU", "China Eastern"),
    ("CHH", "HU", "Hainan Airlines"),
    ("XAM", "MF", "Xiamen Airlines"),
    ("SHQ", "SC", "Shandong Airlines"),
    ("CXA", "KN", "China United Airlines"),
    ("JAL", "JL", "Japan Airlines"),
    ("ANA", "NH", "All Nippon Airways"),
    ("JJP", "GK", "Jetstar Japan"),
    ("KAL", "KE", "Korean Air"),
    ("AAR", "OZ", "Asiana Airlines"),
    ("SIA", "SQ", "Singapore Airlines"),
    ("SLK", "MI", "SilkAir"),
    ("TGW", "TR", "Scoot"),
    ("MAS", "MH", "Malaysia Airlines"),
    ("AXM", "D7", "AirAsia X"),
    ("AIQ", "QZ", "AirAsia"),
    ("IAW", "BI", "Royal Brunei"),
    ("THA", "TG", "Thai Airways"),
    ("TVJ", "VZ", "Thai Vietjet"),
    ("GAR", "GA", "Garuda Indonesia"),
    ("LNI", "JT", "Lion Air"),
    ("BTK", "ID", "Batik Air"),
    ("VJC", "VJ", "Vietjet Air"),
    ("HVN", "VN", "Vietnam Airlines"),
    ("PAL", "PR", "Philippine Airlines"),
    ("CEB", "5J", "Cebu Pacific"),
    ("QFA", "QF", "Qantas"),
    ("JST", "JQ", "Jetstar"),
    ("VOZ", "VA", "Virgin Australia"),
    ("ANZ", "NZ", "Air New Zealand"),
    ("AIX", "IX", "Air India Express"),
    ("AIC", "AI", "Air India"),
    ("IGO", "6E", "IndiGo"),
    ("SEJ", "SG", "SpiceJet"),

    # ── Middle East & Africa ─────────────────────────────────────────────────
    ("SVA", "SV", "Saudia"),
    ("FDB", "FZ", "flydubai"),
    ("ABY", "G9", "Air Arabia"),
    ("OAL", "OA", "Olympic Air"),
    ("MSR", "MS", "EgyptAir"),
    ("ETH", "ET", "Ethiopian Airlines"),
    ("KQA", "KQ", "Kenya Airways"),
    ("SAA", "SA", "South African Airways"),

    # ── Latin America ─────────────────────────────────────────────────────────
    ("LAN", "LA", "LATAM Airlines"),
    ("TAM", "JJ", "LATAM Brasil"),
    ("GLO", "G3", "Gol Linhas Aéreas"),
    ("AZU", "AD", "Azul Brazilian"),
    ("AVA", "AV", "Avianca"),
    ("VOI", "Y4", "Volaris"),
    ("AMX", "AM", "Aeromexico"),
    ("VIV", "VB", "VivaAerobus"),

    # ── Russia / CIS ─────────────────────────────────────────────────────────
    ("AFL", "SU", "Aeroflot"),
    ("SDM", "FV", "Rossiya Airlines"),
    ("SVP", "UT", "UTair"),
]


# ─── Conversion helpers ───────────────────────────────────────────────────────

def rgb_to_rgb565(r: int, g: int, b: int) -> int:
    """Convert 8-bit RGB to 16-bit RGB565."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def image_to_rgb565_blob(img: Image.Image, width: int, height: int) -> bytes:
    """
    Resize img to (width, height) using high-quality downsampling, then
    convert each pixel to RGB565. Fully-transparent pixels (alpha < 16) are
    encoded as TRANSPARENT_RGB565 (0xF81F) so the display can skip them.
    Returns a bytes object of length width*height*2 (little-endian uint16_t).
    """
    # Resize to target dimensions with high-quality Lanczos filter.
    img = img.convert("RGBA")
    img = img.resize((width, height), Image.LANCZOS)

    blob = bytearray()
    pixels = img.load()

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a < 16:
                pixel16 = TRANSPARENT_RGB565
            else:
                # Blend onto black background proportionally to alpha.
                r = int(r * a / 255)
                g = int(g * a / 255)
                b = int(b * a / 255)
                pixel16 = rgb_to_rgb565(r, g, b)
                # Avoid accidentally hitting the transparency sentinel.
                if pixel16 == TRANSPARENT_RGB565:
                    pixel16 = 0xF820  # slightly off-magenta, visually identical
            blob += struct.pack('<H', pixel16)

    return bytes(blob)


# ─── Logo download ────────────────────────────────────────────────────────────

LOGO_URL_TEMPLATE = (
    "https://content.airhex.com/content/logos/"
    "airlines_{iata}_{width}_{height}_r.png"
    "?theme=dark"
)


def fetch_logo_png(iata: str, width: int, height: int) -> bytes | None:
    """Download a PNG logo from Airhex CDN. Returns raw PNG bytes or None."""
    url = LOGO_URL_TEMPLATE.format(iata=iata, width=width, height=height)
    try:
        resp = requests.get(url, timeout=10)
        if resp.status_code == 200 and resp.headers.get("Content-Type", "").startswith("image"):
            return resp.content
        else:
            return None
    except requests.RequestException as e:
        print(f"    Network error: {e}")
        return None


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Build local RGB565 airline logo blobs for FlightWall ESP32 firmware."
    )
    parser.add_argument("--width",  type=int, default=DEFAULT_WIDTH,
                        help=f"Logo width in pixels (default: {DEFAULT_WIDTH})")
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT,
                        help=f"Logo height in pixels (default: {DEFAULT_HEIGHT})")
    parser.add_argument("--out",    type=Path, default=DEFAULT_OUT,
                        help=f"Output directory (default: {DEFAULT_OUT})")
    parser.add_argument("--skip-existing", action="store_true",
                        help="Skip airlines whose .bin file already exists")
    args = parser.parse_args()

    out_dir: Path = args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    width:  int = args.width
    height: int = args.height
    expected_bytes = width * height * 2

    print(f"FlightWall Logo Builder")
    print(f"  Output dir  : {out_dir}")
    print(f"  Logo size   : {width}x{height} px  ({expected_bytes} bytes each)")
    print(f"  Airlines    : {len(AIRLINE_LIST)}")
    print(f"  Total max   : {len(AIRLINE_LIST) * expected_bytes / 1024:.1f} KB")
    print()

    success = 0
    skipped = 0
    failed  = 0
    failed_list = []

    for idx, (icao, iata, name) in enumerate(AIRLINE_LIST, start=1):
        out_path = out_dir / f"{icao.upper()}.bin"
        prefix = f"[{idx:3d}/{len(AIRLINE_LIST)}] {icao} ({iata}) {name}"

        if args.skip_existing and out_path.exists():
            print(f"{prefix} — skipped (exists)")
            skipped += 1
            continue

        print(f"{prefix} — downloading...", end=" ", flush=True)
        png_bytes = fetch_logo_png(iata, width * 4, height * 4)  # fetch 4x then downsample

        if png_bytes is None:
            print("FAILED (no logo)")
            failed += 1
            failed_list.append(f"{icao}/{iata} {name}")
            time.sleep(REQUEST_DELAY_S)
            continue

        try:
            img = Image.open(io.BytesIO(png_bytes))
            blob = image_to_rgb565_blob(img, width, height)
            assert len(blob) == expected_bytes, f"blob size {len(blob)} != {expected_bytes}"
            out_path.write_bytes(blob)
            print(f"OK ({len(blob)} bytes)")
            success += 1
        except Exception as e:
            print(f"FAILED ({e})")
            failed += 1
            failed_list.append(f"{icao}/{iata} {name}")

        time.sleep(REQUEST_DELAY_S)

    print()
    print("─" * 60)
    print(f"Done.  {success} OK  |  {skipped} skipped  |  {failed} failed")
    if failed_list:
        print("Failed airlines:")
        for f in failed_list:
            print(f"  {f}")

    total_kb = sum(p.stat().st_size for p in out_dir.glob("*.bin")) / 1024
    print(f"Total logo data: {total_kb:.1f} KB in {out_dir}")
    print()
    print("Next steps:")
    print("  cd firmware")
    print("  pio run --target uploadfs")

if __name__ == "__main__":
    main()
