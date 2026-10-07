#!/usr/bin/env python3
import subprocess
import os
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    res_dir = os.path.join(project_root, "cpp", "res")
    build_dir = os.path.join(project_root, "build", "icons")

    os.makedirs(res_dir, exist_ok=True)
    os.makedirs(build_dir, exist_ok=True)

    # 1. High-resolution SVG (256x256) - Beautiful full details, NO shadow filter (100% transparent background)
    svg_large = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="bgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0066E6"/>
      <stop offset="50%" stop-color="#0078D7"/>
      <stop offset="100%" stop-color="#00B4D8"/>
    </linearGradient>
    <linearGradient id="cornerGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="100%" stop-color="#99EEFF"/>
    </linearGradient>
    <linearGradient id="lensGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0F172A"/>
      <stop offset="100%" stop-color="#1E293B"/>
    </linearGradient>
  </defs>

  <!-- Background Tile (Squircle with 100% transparent corners outside) -->
  <rect x="18" y="18" width="220" height="220" rx="48" fill="url(#bgGrad)"/>
  <rect x="19" y="19" width="218" height="218" rx="47" fill="none" stroke="#FFFFFF" stroke-width="2" opacity="0.3"/>

  <!-- Viewfinder Frame Border -->
  <rect x="52" y="52" width="152" height="152" rx="14" fill="none" stroke="#FFFFFF" stroke-width="4" stroke-dasharray="10 8" opacity="0.45"/>

  <!-- Thick Corner Crop Brackets -->
  <path d="M 44 80 L 44 48 A 4 4 0 0 1 48 44 L 80 44" fill="none" stroke="url(#cornerGrad)" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 176 44 L 208 44 A 4 4 0 0 1 212 48 L 212 80" fill="none" stroke="url(#cornerGrad)" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 44 176 L 44 208 A 4 4 0 0 0 48 212 L 80 212" fill="none" stroke="url(#cornerGrad)" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 176 212 L 208 212 A 4 4 0 0 0 212 208 L 212 176" fill="none" stroke="url(#cornerGrad)" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>

  <!-- Center Camera Shutter / Lens -->
  <circle cx="128" cy="128" r="44" fill="url(#lensGrad)"/>
  <circle cx="128" cy="128" r="44" fill="none" stroke="#FFFFFF" stroke-width="6"/>
  <circle cx="128" cy="128" r="30" fill="#00A2ED" opacity="0.65"/>
  <circle cx="128" cy="128" r="18" fill="#FFFFFF"/>

  <!-- Glint Highlight -->
  <circle cx="116" cy="116" r="6" fill="#FFFFFF" opacity="0.9"/>
  <circle cx="138" cy="138" r="3" fill="#FFFFFF" opacity="0.6"/>

  <!-- Red Recording Indicator / Status Dot -->
  <circle cx="182" cy="74" r="8" fill="#FF4444"/>
  <circle cx="182" cy="74" r="8" fill="none" stroke="#FFFFFF" stroke-width="2.5"/>
</svg>'''

    # 2. Compact simplified SVG for 32x32 / 24x24 - High-contrast, no micro-detailing/noise
    svg_compact32 = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" width="32" height="32">
  <defs>
    <linearGradient id="bgS" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0066E6"/>
      <stop offset="100%" stop-color="#00B4D8"/>
    </linearGradient>
  </defs>
  <rect x="2" y="2" width="28" height="28" rx="6" fill="url(#bgS)"/>

  <!-- Bold Corner Brackets -->
  <path d="M 6 11 L 6 6 L 11 6" fill="none" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 21 6 L 26 6 L 26 11" fill="none" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 6 21 L 6 26 L 11 26" fill="none" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 21 26 L 26 26 L 26 21" fill="none" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>

  <!-- Bold Center Lens -->
  <circle cx="16" cy="16" r="5.5" fill="#1E293B"/>
  <circle cx="16" cy="16" r="5.5" fill="none" stroke="#FFFFFF" stroke-width="1.8"/>
  <circle cx="16" cy="16" r="2.5" fill="#00E5FF"/>

  <!-- Red Status Dot -->
  <circle cx="23" cy="9" r="1.8" fill="#FF3333"/>
</svg>'''

    # 3. Compact pixel-aligned SVG for 16x16 - Ultra-clear at 16x16
    svg_compact16 = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16">
  <defs>
    <linearGradient id="bg16" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#0066E6"/>
      <stop offset="100%" stop-color="#00B4D8"/>
    </linearGradient>
  </defs>
  <rect x="1" y="1" width="14" height="14" rx="3.5" fill="url(#bg16)"/>

  <!-- Corner Brackets -->
  <path d="M 3 6 L 3 3 L 6 3" fill="none" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 10 3 L 13 3 L 13 6" fill="none" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 3 10 L 3 13 L 6 13" fill="none" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 10 13 L 13 13 L 13 10" fill="none" stroke="#FFFFFF" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>

  <!-- Center Dot -->
  <circle cx="8" cy="8" r="2.2" fill="#FFFFFF"/>
  <circle cx="8" cy="8" r="1.2" fill="#0066E6"/>
</svg>'''

    # Write SVG files
    svg_large_path = os.path.join(res_dir, "icon.svg")
    with open(svg_large_path, "w", encoding="utf-8") as f:
        f.write(svg_large)

    svg_compact32_path = os.path.join(build_dir, "compact32.svg")
    with open(svg_compact32_path, "w", encoding="utf-8") as f:
        f.write(svg_compact32)

    svg_compact16_path = os.path.join(build_dir, "compact16.svg")
    with open(svg_compact16_path, "w", encoding="utf-8") as f:
        f.write(svg_compact16)

    # Render PNGs using ImageMagick with background none (transparent)
    png_256 = os.path.join(res_dir, "icon.png")
    subprocess.run(["magick", "-background", "none", svg_large_path, "-resize", "256x256", png_256], check=True)

    png_128 = os.path.join(build_dir, "icon128.png")
    subprocess.run(["magick", "-background", "none", svg_large_path, "-resize", "128x128", png_128], check=True)

    png_64 = os.path.join(build_dir, "icon64.png")
    subprocess.run(["magick", "-background", "none", svg_large_path, "-resize", "64x64", png_64], check=True)

    png_48 = os.path.join(build_dir, "icon48.png")
    subprocess.run(["magick", "-background", "none", svg_large_path, "-resize", "48x48", png_48], check=True)

    png_32 = os.path.join(build_dir, "icon32.png")
    subprocess.run(["magick", "-background", "none", svg_compact32_path, "-resize", "32x32", png_32], check=True)

    png_24 = os.path.join(build_dir, "icon24.png")
    subprocess.run(["magick", "-background", "none", svg_compact32_path, "-resize", "24x24", png_24], check=True)

    png_16 = os.path.join(build_dir, "icon16.png")
    subprocess.run(["magick", "-background", "none", svg_compact16_path, "-resize", "16x16", png_16], check=True)

    # Pack into multi-resolution WinScreen.ico using standard Windows Vista+ PNG-compressed ICO format
    # This guarantees 100% true 32-bit RGBA alpha transparency in Windows 10/11 Explorer, Desktop, and Taskbar
    import struct
    ico_path = os.path.join(res_dir, "WinScreen.ico")
    images = [
        (256, png_256),
        (128, png_128),
        (64, png_64),
        (48, png_48),
        (32, png_32),
        (24, png_24),
        (16, png_16)
    ]

    num_images = len(images)
    header = struct.pack("<HHH", 0, 1, num_images)

    entries = []
    png_datas = []
    offset = 6 + num_images * 16
    for size, path in images:
        with open(path, "rb") as f:
            data = f.read()
        png_datas.append(data)
        w = 0 if size == 256 else size
        h = 0 if size == 256 else size
        entry = struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(data), offset)
        entries.append(entry)
        offset += len(data)

    with open(ico_path, "wb") as f:
        f.write(header)
        for e in entries:
            f.write(e)
        for data in png_datas:
            f.write(data)

    print(f"Successfully generated {ico_path} (size: {os.path.getsize(ico_path)} bytes) and {png_256}")

if __name__ == "__main__":
    main()
