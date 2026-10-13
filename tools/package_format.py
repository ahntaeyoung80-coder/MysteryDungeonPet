"""MDPA v1 writer shared by the fixture producer and future A-1 packer.

No ROM extraction here. The runtime is the authoritative validating consumer.
"""
import struct


def encode_package(package):
    out = bytearray()

    def u32(n):
        out.extend(struct.pack('<I', n))

    def i32(n):
        out.extend(struct.pack('<i', n))

    def string(s):
        data = s.encode('utf-8')
        u32(len(data))
        out.extend(data)

    for n in (0x4150444D, 1, package['assetVersion'], package['tickHz'],
              package['width'], package['height'], package['pngBytes'], package['sourceKind'], package.get('shadowSize', 0)):
        u32(n)
    for field in ('atlasName', 'pngSha256', 'assetKey', 'provenance'):
        string(package[field])
    u32(len(package['clips']))
    for clip in package['clips']:
        u32(clip['id'])
        i32(clip.get('sourceWanId', -1))
        i32(clip.get('aliasOf', -1))
        u32(clip.get('rawFlags', 0))
        u32(clip.get('speedNumerator', 1))
        u32(clip.get('speedDenominator', 1))
        string(clip['name'])
        string(clip.get('sourceMetadata', ''))
        u32(len(clip['rows']))
        for row in clip['rows']:
            u32(len(row))
            for frame in row:
                for n in (*frame['rect'], *frame['anchor'], *frame['shadow']):
                    i32(n)
                u32(frame['ticks'])
                u32(len(frame['markers']))
                for marker in frame['markers']:
                    u32(marker['kind'])
                    for n in marker['point']:
                        i32(n)
                for n in (*frame.get('shadowRect', [0, 0, 0, 0]), *frame.get('cropOrigin', [0, 0])):
                    i32(n)
                u32(frame.get('rawFlag', 0xffffffff))
                for n in (*frame.get('rawOffset', [0, 0]), *frame.get('rawShadow', [0, 0])):
                    i32(n)
                u32(frame.get('events', 0))
                string(frame.get('sourceMetadata', ''))
    return bytes(out)
