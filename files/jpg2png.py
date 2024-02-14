import os
from PIL import Image

for filename in os.listdir('.'):
    if filename.endswith('.jpg'):
        img = Image.open(filename)
        img.save(filename[:-4] + '.png')
