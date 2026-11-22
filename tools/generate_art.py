"""Author the original Fodder mesh, pixel and sound assets.

Run with Python 3 to regenerate the files shipped in assets/. The build copies
them verbatim so that players can replace them in a content pack.
"""
from math import exp, pi, sin
from pathlib import Path
import random
import struct
import wave
import zlib

ROOT = Path(__file__).resolve().parents[1] / "assets"
PACK = ROOT / "packs" / "fantasy"


class Mesh:
    def __init__(self):
        self.vertices = []

    def face(self, points, color, shade=1.0):
        dye = tuple(component * shade for component in color[:3]) + (color[3],)
        for index in range(1, len(points) - 1):
            for point in (points[0], points[index], points[index + 1]):
                self.vertices.append((*point, *dye))

    def box(self, start, end, color):
        x, y, z = start
        X, Y, Z = end
        v = [(x,y,z), (X,y,z), (X,y,Z), (x,y,Z), (x,Y,z), (X,Y,z), (X,Y,Z), (x,Y,Z)]
        for ids, shade in [
            ((0,1,2,3), .55), ((4,5,6,7), 1), ((0,1,5,4), .92),
            ((2,3,7,6), .74), ((0,3,7,4), .62), ((1,2,6,5), .80)
        ]:
            self.face([v[index] for index in ids], color, shade)

    def plate(self, x, y, front, back, width, height, color):
        rim = [(-.80,.5), (.80,.5), (1,.25), (.78,-.31), (0,-.65), (-.78,-.31), (-1,.25)]
        points = [(x + a * width, y + b * height, front) for a, b in rim]
        rear = [(a, b, back) for a, b, _ in points]
        self.face(points, color)
        self.face(rear, color, .63)
        for i in range(len(points)):
            j = (i + 1) % len(points)
            self.face([points[i], points[j], rear[j], rear[i]], color, .69)

    def save(self, name):
        text = [f"FODDERM1 {len(self.vertices)}"]
        text.extend(" ".join(f"{value:.4f}" for value in row) for row in self.vertices)
        (PACK / "models" / f"{name}.mesh").write_text("\n".join(text) + "\n", encoding="ascii")


STEEL = (.66, .70, .63, 0)
MAIL = (.37, .43, .42, 0)
GILT = (.78, .62, .37, 0)
CLOTH = (.88, .88, .80, 1)
LEATHER = (.30, .22, .16, 0)
DARK = (.13, .16, .16, 0)


def sword(mesh, x=0.0, y=0.0, z=0.0, scale=1.0):
    def point(u, v, w):
        return x + u * scale, y + v * scale, z + w * scale

    mesh.box(point(-.038,-.25,-.039), point(.038,.02,.039), LEATHER)
    mesh.box(point(-.16,-.04,-.05), point(.16,.02,.05), GILT)
    mesh.box(point(-.055,-.28,-.047), point(.055,-.24,.047), GILT)
    hilt_l, hilt_r = point(-.075,.025,0), point(.075,.025,0)
    neck_l, neck_r = point(-.055,.86,0), point(.055,.86,0)
    ridge, ridge_neck, tip = point(0,.025,-.026), point(0,.87,-.021), point(0,1.04,0)
    mesh.face([hilt_l,ridge,ridge_neck,neck_l,tip], (.70,.77,.74,0))
    mesh.face([ridge,hilt_r,neck_r,tip,ridge_neck], (.49,.59,.59,0))
    mesh.face([hilt_l,hilt_r,neck_r,tip,neck_l], (.56,.64,.62,0))


def shield(mesh, x=0.0, y=0.0, z=0.0, scale=1.0):
    mesh.plate(x, y, z - .095*scale, z, .40*scale, .78*scale, GILT)
    mesh.plate(x, y + .007*scale, z - .105*scale, z - .096*scale,
               .343*scale, .677*scale, CLOTH)
    mesh.plate(x, y + .05*scale, z - .14*scale, z - .11*scale,
               .085*scale, .19*scale, STEEL)


def models():
    held = Mesh()
    sword(held)
    held.save("sword")
    held = Mesh()
    shield(held)
    held.save("shield")
    levy = Mesh()
    for side in [-1, 1]:
        x = side * .14
        levy.box((x-.083,.02,-.13),(x+.083,.19,.09), LEATHER)
        levy.box((x-.074,.18,-.076),(x+.074,.78,.074), MAIL)
        levy.box((x-.084,.34,-.094),(x+.084,.46,-.054), STEEL)
    levy.box((-.224,.65,-.137),(.224,1.23,.145), CLOTH)
    levy.box((-.23,.76,-.15),(.23,.84,.152), LEATHER)
    levy.box((-.071,.754,-.172),(.071,.84,-.149), GILT)
    levy.box((-.26,1.15,-.15),(.26,1.36,.151), MAIL)
    for side in [-1, 1]:
        x = side * .29
        levy.box((x-.104,1.14,-.114),(x+.104,1.33,.114), STEEL)
        levy.box((x-.064,.91,-.075),(x+.064,1.17,.083), CLOTH)
    levy.box((-.111,1.29,-.09),(.111,1.50,.092), LEATHER)
    levy.box((-.14,1.43,-.15),(.14,1.67,.12), STEEL)
    levy.box((-.112,1.64,-.109),(.112,1.76,.088), MAIL)
    levy.box((-.118,1.538,-.16),(.118,1.583,-.143), DARK)
    levy.box((-.022,1.44,-.17),(.022,1.646,-.148), GILT)
    levy.box((-.062,1.74,-.057),(.065,1.84,.12), CLOTH)
    sword(levy, .39, .99, -.20, .63)
    shield(levy, -.31, .96, -.16, .72)
    levy.save("levy")


def tga(path, width, height, pixels):
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 8)
    path.write_bytes(header + bytes(c for r, g, b, a in reversed(pixels) for c in (b, g, r, a)))


def pixels():
    # The sprites are drawn top-down. Reverse the rows for TGA's bottom origin.
    def write(path, width, height, picture):
        surface = [picture(width - 1 - x, y) for y in range(height) for x in range(width)]
        tga(path, width, height, surface)

    def soldier(x, y):
        outline = (35, 45, 48, 255)
        armor = (151, 160, 151, 255)
        cloth = (190, 182, 158, 255)
        bronze = (199, 164, 104, 255)
        if 4 <= x <= 24 and y == 36:
            return (37, 45, 43, 125)
        if 11 <= x <= 16 and 2 <= y <= 4:
            return cloth
        if 10 <= x <= 17 and 5 <= y <= 11:
            return outline if y in (8,11) or x in (10,17) else armor
        if 8 <= x <= 20 and 12 <= y <= 26:
            return outline if x in (8,20) or y in (12,22) else cloth
        if 21 <= x <= 22 and 9 <= y <= 23:
            return bronze if y in (21,22) else armor
        if 4 <= x <= 11 and 16 <= y <= 29 and (y <= 25 or abs(x - 7) < 3):
            return bronze if x in (4,11) or y in (16,29) else cloth
        if y >= 27 and y <= 35 and (10 <= x <= 13 or 16 <= x <= 19):
            return outline if y >= 33 else armor
        return (0, 0, 0, 0)

    def banner(x, y):
        if x in (1,30) or y in (2,44):
            return (213, 181, 106, 255)
        if y > 46 - abs(x - 16) // 2:
            return (0, 0, 0, 0)
        center = y - 9
        if 0 <= center <= 28:
            if abs(x - 15) <= 1 or abs(x - (9 + center // 5)) <= 1 or abs(x - (22 - center // 5)) <= 1:
                return (246, 231, 170, 255)
        return (130 + x % 4 * 6, 124 + x % 3 * 6, 96, 255)

    write(PACK / "sprites" / "ranks.tga", 28, 40, soldier)
    write(PACK / "textures" / "banner.tga", 32, 48, banner)


FONT = {
    "0": "0e 11 13 15 19 11 0e", "1": "04 0c 04 04 04 04 0e",
    "2": "0e 11 01 02 04 08 1f", "3": "1e 01 01 0e 01 01 1e",
    "4": "02 06 0a 12 1f 02 02", "5": "1f 10 10 1e 01 01 1e",
    "6": "06 08 10 1e 11 11 0e", "7": "1f 01 02 04 08 08 08",
    "8": "0e 11 11 0e 11 11 0e", "9": "0e 11 11 0f 01 02 0c",
    "A": "0e 11 11 1f 11 11 11", "B": "1e 11 11 1e 11 11 1e",
    "C": "0e 11 10 10 10 11 0e", "D": "1e 11 11 11 11 11 1e",
    "E": "1f 10 10 1e 10 10 1f", "F": "1f 10 10 1e 10 10 10",
    "G": "0e 11 10 17 11 11 0f", "H": "11 11 11 1f 11 11 11",
    "I": "0e 04 04 04 04 04 0e", "J": "07 02 02 02 02 12 0c",
    "K": "11 12 14 18 14 12 11", "L": "10 10 10 10 10 10 1f",
    "M": "11 1b 15 15 11 11 11", "N": "11 19 15 13 11 11 11",
    "O": "0e 11 11 11 11 11 0e", "P": "1e 11 11 1e 10 10 10",
    "Q": "0e 11 11 11 15 12 0d", "R": "1e 11 11 1e 14 12 11",
    "S": "0f 10 10 0e 01 01 1e", "T": "1f 04 04 04 04 04 04",
    "U": "11 11 11 11 11 11 0e", "V": "11 11 11 11 11 0a 04",
    "W": "11 11 11 15 15 1b 11", "X": "11 11 0a 04 0a 11 11",
    "Y": "11 11 0a 04 04 04 04", "Z": "1f 01 02 04 08 10 1f",
    ":": "00 04 04 00 04 04 00", ".": "00 00 00 00 00 04 04",
    "-": "00 00 00 1f 00 00 00", "/": "01 01 02 04 08 10 10",
    "+": "00 04 04 1f 04 04 00", "?": "0e 11 01 02 04 00 04",
    "'": "04 04 08 00 00 00 00", "!": "04 04 04 04 04 00 04"
}


def audio():
    rate = 22050
    rng = random.Random(611)
    for name, seconds in [("clash", .36), ("horn", 2.28), ("warfield", 5.0)]:
        samples = []
        moving = 0
        for i in range(int(rate * seconds)):
            t = i / rate
            noise = rng.uniform(-1, 1)
            if name == "clash":
                value = (0.16*noise + sum(sin(2*pi*f*t)*.19 for f in (603,911,1497))) * exp(-t*16)
            elif name == "horn":
                envelope = min(1, t*12) * min(1, (seconds-t)*2.6)
                focus = t + .0009*sin(t*29)
                value = sum(sin(2*pi*f*focus)*gain for f, gain in [(98,.18),(196,.095),(294,.037)]) * envelope
            else:
                moving = .985*moving + .015*noise
                value = (.09*sin(2*pi*49*t) + .046*sin(2*pi*66*t))*(.6+.4*sin(2*pi*.4*t)) + .44*moving
            samples.append(struct.pack("<h", int(value * 30000)))
        with wave.open(str(PACK / "audio" / f"{name}.wav"), "wb") as output:
            output.setparams((1,2,rate,0,"NONE","not compressed"))
            output.writeframes(b"".join(samples))


def emblem():
    def chunk(name, data):
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))

    surface = bytearray()
    for y in range(32):
        surface.append(0)
        for x in range(32):
            bar = 8 <= x <= 11 or 14 <= x <= 17 or 20 <= x <= 23
            top = 5 if 14 <= x <= 17 else 9
            bottom = 26 if 14 <= x <= 17 else 23
            surface.extend((221, 169, 108, 255) if bar and top <= y <= bottom else (20, 32, 30, 255))
    header = struct.pack(">IIBBBBB", 32, 32, 8, 6, 0, 0, 0)
    (ROOT / "ui" / "emblem.png").write_bytes(
        bytes.fromhex("89504e470d0a1a0a") + chunk(b"IHDR", header) +
        chunk(b"IDAT", zlib.compress(surface)) + chunk(b"IEND", b"")
    )


if __name__ == "__main__":
    models()
    emblem()
    pixels()
    audio()
    (ROOT / "ui" / "font.txt").write_text(
        "".join(f"{ord(letter)} {rows}\n" for letter, rows in sorted(FONT.items())), encoding="ascii"
    )
