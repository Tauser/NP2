"""Small synthetic JPEG corpus for the fixed-budget cover decoder host test."""
from pathlib import Path
import sys
from PIL import Image

directory = Path(sys.argv[1])
directory.mkdir(parents=True, exist_ok=True)
for name, size, progressive in (("cover64", (64, 64), False),
                                ("cover301", (301, 300), False),
                                ("cover300", (300, 300), False),
                                ("cover-progressive", (64, 64), True)):
    Image.new("RGB", size, (255, 0, 0)).save(directory / (name + ".jpg"), quality=80, progressive=progressive)

image = Image.new("RGB", (300, 300))
image.putdata([(x*255//299, y*255//299, 40) for y in range(300) for x in range(300)])
image.save(directory / "cover300-gradient.jpg", quality=95, subsampling=0)
