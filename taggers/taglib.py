#!/usr/bin/env python

from struct import pack, unpack

###############################################################################
# color crap
###############################################################################

palette = [0xbccbde, 0xc2dde6, 0xe6e9f0, 0x431c5d, 0xe05915, 0xcdd422]

def rgbDecomp(color):
    return [(color&0xFF0000)>>16, (color&0xFF00)>>8, color&0xFF]

def rgbComp(r,g,b):
    return (r<<16)|(g<<8)|b;

def adjValue(color, coeff):
    [r,g,b] = map(lambda x: x/255.0, rgbDecomp(color))
    [h,s,v] = colorsys.rgb_to_hsv(r,g,b)
    v = min(1, coeff*v)
    [r,g,b] = map(lambda x: int(x*255), colorsys.hsv_to_rgb(h,s,v))
    return rgbComp(r,g,b)

def adjSaturation(color, coeff):
    [r,g,b] = map(lambda x: x/255.0, rgbDecomp(color))
    [h,s,v] = colorsys.rgb_to_hsv(r,g,b)
    s = min(1, coeff*s)
    [r,g,b] = map(lambda x: int(x*255), colorsys.hsv_to_rgb(h,s,v))
    return rgbComp(r,g,b)

def adjHue(color, addend):
    [r,g,b] = map(lambda x: x/255.0, rgbDecomp(color))
    [h,s,v] = colorsys.rgb_to_hsv(r,g,b)
    h = min(1, h+addend)
    [r,g,b] = map(lambda x: int(x*255), colorsys.hsv_to_rgb(h,s,v))
    return rgbComp(r,g,b)
    
def colorFromBytes(data):
    color_lookup = [ \
        0x772277, 0x752277, 0x732277, 0x722378, 0x702378, 0x6F2378, 0x6D2479, 0x6B2479, \
        0x6A257A, 0x68257A, 0x67257A, 0x65267B, 0x64267B, 0x62277C, 0x60277C, 0x5F277C, \
        0x5D287D, 0x5C287D, 0x5A297E, 0x58297E, 0x57297E, 0x552A7F, 0x542A7F, 0x522B80, \
        0x512B80, 0x4F2B80, 0x4D2C81, 0x4C2C81, 0x4A2D82, 0x492D82, 0x472D82, 0x452E83, \
        0x442E83, 0x422F84, 0x412F84, 0x3F2F84, 0x3E3085, 0x3C3085, 0x3A3186, 0x393186, \
        0x373186, 0x363287, 0x343287, 0x333388, 0x323388, 0x313389, 0x30348A, 0x2F348B, \
        0x2F348B, 0x2E358C, 0x2D358D, 0x2C368E, 0x2B368F, 0x2B368F, 0x2A3790, 0x293791, \
        0x283892, 0x273893, 0x273893, 0x263994, 0x253995, 0x243A96, 0x233A97, 0x233A97, \
        0x223B98, 0x213B99, 0x203C9A, 0x203C9A, 0x1F3C9B, 0x1E3D9C, 0x1D3D9D, 0x1C3E9E, \
        0x1C3E9E, 0x1B3E9F, 0x1A3FA0, 0x193FA1, 0x1840A2, 0x1840A2, 0x1740A3, 0x1641A4, \
        0x1541A5, 0x1442A6, 0x1442A6, 0x1342A7, 0x1243A8, 0x1143A9, 0x1144AA, 0x1246A6, \
        0x1448A2, 0x154B9F, 0x174D9B, 0x184F98, 0x1A5294, 0x1C5491, 0x1D568D, 0x1F5989, \
        0x205B86, 0x225E82, 0x23607F, 0x25627B, 0x276578, 0x286774, 0x2A6971, 0x2B6C6D, \
        0x2D6E69, 0x2F7166, 0x307362, 0x32755F, 0x33785B, 0x357A58, 0x367C54, 0x387F51, \
        0x3A814D, 0x3B8449, 0x3D8646, 0x3E8842, 0x408B3F, 0x428D3B, 0x438F38, 0x459234, \
        0x469431, 0x48972D, 0x499929, 0x4B9B26, 0x4D9E22, 0x4EA01F, 0x50A21B, 0x51A518, \
        0x53A714, 0x55AA11, 0x58A510, 0x5CA110, 0x609D0F, 0x64990F, 0x67950E, 0x6B910E, \
        0x6F8D0E, 0x73890D, 0x77850D, 0x7A810C, 0x7E7D0C, 0x82790C, 0x86750B, 0x8A710B, \
        0x8D6D0A, 0x91690A, 0x95650A, 0x996109, 0x9C5D09, 0xA05908, 0xA45508, 0xA85008, \
        0xAC4C07, 0xAF4807, 0xB34406, 0xB74006, 0xBB3C06, 0xBF3805, 0xC23405, 0xC63004, \
        0xCA2C04, 0xCE2804, 0xD12403, 0xD52003, 0xD91C02, 0xDD1802, 0xE11402, 0xE41001, \
        0xE80C01, 0xEC0800, 0xF00400, 0xF40000, 0xF40500, 0xF40A00, 0xF41001, 0xF51501, \
        0xF51B02, 0xF52002, 0xF52503, 0xF62B03, 0xF63004, 0xF63604, 0xF63B05, 0xF74005, \
        0xF74606, 0xF74B06, 0xF75107, 0xF85607, 0xF85B08, 0xF86108, 0xF86609, 0xF96C09, \
        0xF9710A, 0xF9760A, 0xFA7C0A, 0xFA810B, 0xFA870B, 0xFA8C0C, 0xFB910C, 0xFB970D, \
        0xFB9C0D, 0xFBA20E, 0xFCA70E, 0xFCAC0F, 0xFCB20F, 0xFCB710, 0xFDBD10, 0xFDC211, \
        0xFDC711, 0xFDCD12, 0xFED212, 0xFED813, 0xFEDD13, 0xFFE314, 0xFEE319, 0xFEE41F, \
        0xFEE524, 0xFEE52A, 0xFEE62F, 0xFEE735, 0xFEE73B, 0xFEE840, 0xFEE946, 0xFEE94B, \
        0xFEEA51, 0xFEEB57, 0xFEEB5C, 0xFEEC62, 0xFEED67, 0xFEED6D, 0xFEEE73, 0xFEEF78, \
        0xFEEF7E, 0xFEF083, 0xFEF189, 0xFEF18F, 0xFEF294, 0xFEF39A, 0xFEF39F, 0xFEF4A5, \
        0xFEF5AB, 0xFEF5B0, 0xFEF6B6, 0xFEF7BB, 0xFEF7C1, 0xFEF8C7, 0xFEF9CC, 0xFEF9D2, \
        0xFEFAD7, 0xFEFBDD, 0xFEFBE3, 0xFEFCE8, 0xFEFDEE, 0xFEFDF3, 0xFEFEF9, 0xFEFFFF \
    ]

    avg = int(round(sum(map(ord, list(data))) / len(data)))
    assert avg >= 0 and avg <= 255
    return color_lookup[avg]
        
def colorFromBytesFP(FP, length, rewind=0):
    tmp = FP.tell()
    result = colorFromBytes(FP.read(length));
    if rewind: FP.seek(tmp)
    return result

###############################################################################
# FP crap
###############################################################################
def IsEof(FP):
    answer = False
    temp = FP.tell()
    if FP.read()=='':
        answer = True
    FP.seek(temp)
    return answer

def peek(FP,len):
    value = FP.read(len)
    FP.seek(-len,1)
    return value

def tag(FP, length, comment, rewind=0):
    pos = FP.tell()
    val = FP.read(length)
    print '[0x%X,0x%X) 0x0 %s' % (pos, pos+length, comment)
    if rewind: FP.seek(pos)
    return val

# 8-bit bytes
def int8(FP, name, comment, peek=0):
    value = unpack('b', FP.read(1))[0]
    if peek: FP.seek(-1,1)
    return value

def uint8(FP, peek=0):
    value = unpack('B', FP.read(1))[0]
    if peek: FP.seek(-1,1)
    return value

# 16-bit words, little endian
def int16(FP, peek=0):
    value = unpack('<h', FP.read(2))[0]
    if peek: FP.seek(-2,1)
    return value
def uint16(FP, peek=0):
    value = unpack('<H', FP.read(2))[0]
    if peek: FP.seek(-2,1)
    return value
# 16-bit words, big endian
def INT16(FP, peek=0):
    value = unpack('>h', FP.read(2))[0]
    if peek: FP.seek(-2,1)
    return value
def UINT16(FP, peek=0):
    value = unpack('>H', FP.read(2))[0]
    if peek: FP.seek(-2,1)
    return value
# 32-bit words, little endian
def int32(FP, peek=0):
    value = unpack('<i', FP.read(4))[0]
    if peek: FP.seek(-4,1)
    return value
def uint32(FP, peek=0):
    value = unpack('<I', FP.read(4))[0]
    if peek: FP.seek(-4,1)
    return value
def tagUint32(FP, comment, peek=0):
    pos = FP.tell()
    val = uint32(FP, peek)
    print '[0x%X,0x%X) 0x0 %s=0x%X' % (pos, pos+4, comment, val)
    return val
# 32-bit words, big endian
def INT32(FP, peek=0):
    value = unpack('>i', FP.read(4))[0]
    if peek: FP.seek(-4,1)
    return value
def UINT32(FP, peek=0):
    value = unpack('>I', FP.read(4))[0]
    if peek: FP.seek(-4,1)
    return value
# 64-bit words, little endian
def int64(FP, peek=0):
    value = unpack('<q', FP.read(8))[0]
    if peek: FP.seek(-8,1)
    return value
def uint64(FP, peek=0):
    value = unpack('<Q', FP.read(8))[0]
    if peek: FP.seek(-8,1)
    return value
def tagUint64(FP, comment, peek=0):
    pos = FP.tell()
    val = uint64(FP, peek)
    print '[0x%X,0x%X) 0x0 %s=0x%X' % (pos, pos+8, comment, val)
    return val
# 64-bit words, big endian
def INT64(FP, peek=0):
    value = unpack('>q', FP.read(8))[0]
    if peek: FP.seek(-8,1)
    return value
def UINT64(FP, peek=0):
    value = unpack('>Q', FP.read(8))[0]
    if peek: FP.seek(-8,1)
    return value
# strings
def string(FP, length, peek=0):
    value = unpack('%ds' % length, FP.read(length))[0]
    while(value[-1]=='\x00'):
        value = value[0:-1]
    if peek: FP.seek(-1*length, 1)
    return value
def tagString(FP, length, comment, peek=0):
    pos = FP.tell()
    val = string(FP, length, peek)
    print '[0x%X,0x%X) 0x0 %s \"%s\"' % (pos, pos+length, comment, val)
    return val
