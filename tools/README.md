# FlightWall Tools

## `build_logos.py` — Local airline logo builder

Converts airline logos to RGB565 binary blobs for storage on the ESP32's
LittleFS filesystem, eliminating the need for a CDN at runtime.

### How it works

1. Downloads PNG logos from the free [Airhex CDN](https://airhex.com) at 4×
   resolution for better downsampling quality.
2. Resizes each logo to 24×24 pixels using Lanczos filtering.
3. Converts RGBA pixels to 16-bit RGB565 (the native format for LED matrices).
4. Encodes transparent pixels as `0xF81F` (pure magenta) — the display skips
   these so the black LED background shows through.
5. Writes each logo to `firmware/data/logos/{ICAO}.bin`.

### Setup

```bash
pip install Pillow requests
```

### Run

```bash
# From the repo root:
python tools/build_logos.py

# Or with options:
python tools/build_logos.py --width 24 --height 24 --skip-existing
```

### Upload to ESP32

After building logos, upload the filesystem image:

```bash
cd firmware
pio run --target uploadfs
```

Then flash the firmware as normal:

```bash
pio run --target upload
```

### Adding or removing airlines

Edit the `AIRLINE_LIST` in `build_logos.py`. Each entry is a tuple:

```python
("ICAO", "IATA", "Display name for logging")
```

- **ICAO** becomes the filename (`AAL.bin`) and the lookup key on the device.
- **IATA** is used in the logo download URL.

Run `build_logos.py` again — use `--skip-existing` to only fetch new entries.

### Storage budget

| Airlines | Flash usage |
|----------|-------------|
| 50       | ~56 KB      |
| 100      | ~112 KB     |
| 200      | ~225 KB     |
| 500      | ~562 KB     |

The default set of ~100 airlines uses ~112 KB out of the typical 1.5 MB
LittleFS partition — well within budget.

### Logo not showing?

- Check Serial output for `LocalLogoStore:` messages.
- If `LittleFS mount failed` — run `pio run --target uploadfs`.
- If `No logo for XYZ` — the airline isn't in `AIRLINE_LIST`, or its IATA
  code had no logo on Airhex. Add it to the list and re-run the tool.
- If `Size mismatch` — the `.bin` file is corrupt; delete it and re-run.
