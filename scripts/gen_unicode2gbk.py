import os

pwd = os.path.dirname(os.path.abspath(__file__))
path_gbk2unicode = os.path.join(pwd, '../resources/gbk2unicode.txt')
path_unicode2gbk = os.path.join(pwd, '../resources/unicode2gbk.bin')
# gbk2unicode = {}
# unicode2gbk = {}
# 基本汉字: (0x4E00, 0x9FA5)
unicode2gbk = [0 for _ in range(0x9FA6 - 0x4E00)]
cnt = 0
ignored = 0
with open(path_gbk2unicode, 'r') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        gbk, unicode = line.split()
        gbk, unicode = int(gbk, 16), int(unicode, 16)
        # gbk2unicode[gbk] = unicode
        if 0x9FA5 < unicode:
            print('gbk', hex(gbk), 'unicode', hex(unicode), 'char', bytearray(gbk).decode('gbk'))
            ignored += 1
            continue
        cnt += 1
        unicode2gbk[unicode - 0x4E00] = gbk

print('cnt', cnt, 'ignored', ignored, 'range', 0x9FA6 - 0x4E00, 'size', len(unicode2gbk) * 2)
size = len(unicode2gbk) * 2
print('size', size / 1024, 'KiB')
binary = bytearray()
for c in unicode2gbk:
    # little endian
    binary.append(c & 0xFF)
    binary.append(c >> 8)
with open(path_unicode2gbk, 'wb') as f:
    f.write(binary)
print('write', path_unicode2gbk, len(binary) / 1024, 'KiB')
