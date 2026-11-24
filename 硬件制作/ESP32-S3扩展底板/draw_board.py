# -*- coding: utf-8 -*-
"""
生成「ESP32-S3 扩展底板 v1.0」原理图 + PCB 布局图（PNG/PDF）。
对象：芯路城 ESP32S3 开发板（ESP32-S3-WROOM-1 N16R8，2x22=44Pin，2.54mm）
用途：叠插式引脚转接底板，只做引脚引出 + 电源/接口保护。
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
S = 2  # supersample

WHITE = (255, 255, 255)
GRID = (234, 239, 245)
DARK = (28, 37, 48)
GRAY = (128, 138, 152)
RED = (192, 57, 43)
ORANGE = (206, 118, 18)
TEAL = (18, 118, 138)
GREEN = (26, 140, 80)
BLUE = (36, 92, 200)
PURPLE = (128, 68, 158)
BOXFILL = (253, 240, 240)
BOXEDGE = (176, 64, 64)
LIGHT = (243, 248, 252)
AMBER = (190, 120, 0)
SILK = (40, 40, 40)

_fc = {}
def F(sz, bold=False):
    key = (sz, bold)
    if key not in _fc:
        path = r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc"
        _fc[key] = ImageFont.truetype(path, sz * S, index=0)
    return _fc[key]

def sc(v):
    return int(round(v * S))

class C:
    def __init__(self, W, H, bg=WHITE):
        self.W, self.H = W, H
        self.img = Image.new("RGB", (sc(W), sc(H)), bg)
        self.d = ImageDraw.Draw(self.img)
    def line(self, x1, y1, x2, y2, color=GREEN, w=2):
        self.d.line([sc(x1), sc(y1), sc(x2), sc(y2)], fill=color, width=max(1, int(w * S)))
    def rect(self, x0, y0, x1, y1, fill=None, outline=DARK, w=1, radius=0):
        if radius:
            self.d.rounded_rectangle([sc(x0), sc(y0), sc(x1), sc(y1)], radius=sc(radius),
                                     fill=fill, outline=outline, width=max(1, int(w * S)))
        else:
            self.d.rectangle([sc(x0), sc(y0), sc(x1), sc(y1)], fill=fill, outline=outline,
                             width=max(1, int(w * S)))
    def circle(self, cx, cy, r, fill=None, outline=None, w=1):
        self.d.ellipse([sc(cx - r), sc(cy - r), sc(cx + r), sc(cy + r)], fill=fill,
                       outline=outline, width=max(1, int(w * S)))
    def text(self, x, y, s, f, color=DARK, anchor="lm"):
        self.d.text([sc(x), sc(y)], s, font=f, fill=color, anchor=anchor)
    def grid(self, step=40):
        for x in range(0, self.W, step):
            self.line(x, 0, x, self.H, color=GRID, w=1)
        for y in range(0, self.H, step):
            self.line(0, y, self.W, y, color=GRID, w=1)
    def save(self, name):
        out = self.img.resize((self.W, self.H), Image.LANCZOS)
        out.save(os.path.join(OUT, name), dpi=(200, 200))
        out.save(os.path.join(OUT, name.replace(".png", ".pdf")), "PDF", resolution=200.0)

def comp(c, cx, cy, w, h, ref, val, color=RED, fill=BOXFILL, refsz=15, valsz=11):
    c.rect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, fill=fill, outline=color, w=1.5, radius=4)
    c.text(cx, cy - 7, ref, F(refsz, True), color, "mm")
    c.text(cx, cy + 11, val, F(valsz), DARK, "mm")

# ---------------- pin data ----------------
LEFT = [
    ("3V3", "3V3"), ("3V3", "3V3"), ("RST", "EN"), ("GPIO4", "ADC1_3/TOUCH4"),
    ("GPIO5", "ADC1_4/TOUCH5"), ("GPIO6", "ADC1_5/TOUCH6"), ("GPIO7", "ADC2_6/TOUCH7"),
    ("GPIO15", "ADC2_4/U0RTS"), ("GPIO16", "ADC2_5/U0CTS"), ("GPIO17", "ADC2_6/U1TXD"),
    ("GPIO18", "ADC2_7/U1RXD"), ("GPIO8", "ADC1_7/TOUCH8"), ("GPIO3", "ADC1_2/TOUCH3"),
    ("GPIO46", "LOG"), ("GPIO9", "TOUCH9"), ("GPIO10", "ADC1_9/TOUCH10"),
    ("GPIO11", "ADC2_0/TOUCH11"), ("GPIO12", "ADC2_1/TOUCH12"), ("GPIO13", "ADC2_2/TOUCH13"),
    ("GPIO14", "ADC2_3/TOUCH14"), ("5V0", "5V"), ("GND", "GND"),
]
RIGHT = [
    ("GND", "GND"), ("GPIO43", "U0TXD"), ("GPIO44", "U0RXD"), ("GPIO1", "ADC1_0/TOUCH1"),
    ("GPIO2", "ADC1_1/TOUCH2"), ("GPIO42", "MTMS"), ("GPIO41", "MTDI"), ("GPIO40", "MTDO"),
    ("GPIO39", "MTCK"), ("GPIO38", "FSPIWP"), ("GPIO37", "SPIDQS"), ("GPIO36", "FSPICLK"),
    ("GPIO35", "FSPID"), ("GPIO0", "BOOT"), ("GPIO45", "VSPI"), ("GPIO48", "RGB_LED"),
    ("GPIO47", "SPICLK_P"), ("GPIO21", "RTC"), ("GPIO20", "USB_D+"), ("GPIO19", "USB_D-"),
    ("GND", "GND"), ("GND", "GND"),
]

def net_color(nm):
    if nm == "GND":
        return GRAY
    if nm in ("5V0", "5V_IN", "5V_OUT"):
        return RED
    if nm == "3V3":
        return ORANGE
    if nm.startswith("USB"):
        return BLUE
    if nm == "RST":
        return PURPLE
    return TEAL

# ---------------- schematic ----------------
def draw_schematic():
    c = C(2400, 1760)
    c.grid(40)

    # header
    c.text(60, 52, "ESP32-S3 扩展底板 v1.0 · 原理图", F(34, True), DARK, "lm")
    c.text(60, 98, "对象：芯路城 ESP32S3 开发板（ESP32-S3-WROOM-1 N16R8，2×22=44Pin，2.54mm）"
                   "　｜　叠插式引脚转接：44 脚 1:1 直通 + 电源/接口保护", F(16), GRAY, "lm")
    c.line(60, 128, 2340, 128, color=(200, 210, 222), w=2)

    # ---- connectors J1 / J2 ----
    def draw_conn(x0, title, pins, tag):
        bw = 250
        x1 = x0 + bw
        ytop, dy = 190, 55
        ybot = ytop + 22 * dy
        c.text(x0, 158, title, F(18, True), DARK, "lm")
        c.rect(x0, ytop, x1, ybot, fill=LIGHT, outline=BOXEDGE, w=2, radius=8)
        for i, (nm, alt) in enumerate(pins):
            yy = ytop + i * dy + dy / 2
            # pin number
            c.text(x1 - 12, yy - 13, str(i + 1), F(11), GRAY, "rm")
            # pin name inside
            c.text(x0 + 14, yy - 8, nm, F(16, True), DARK, "lm")
            # alt-function small text (highlight UART TX/RX)
            if alt and alt != "GND" and alt != "5V" and alt != "3V3":
                key = ("TX" in alt) or ("RX" in alt)
                c.text(x0 + 14, yy + 13, alt, F(9, key), GREEN if key else GRAY, "lm")
            # stub + net label outside
            c.line(x1, yy, x1 + 34, yy, color=GREEN, w=2)
            col = net_color(nm)
            c.circle(x1 + 34, yy, 3.2, fill=col)
            c.text(x1 + 44, yy, nm, F(15, True), col, "lm")
        return x1 + 44 + 92  # right extent approx

    draw_conn(150, "J1　1×22 左排（叠插排母 · 长针）", LEFT, "L")
    draw_conn(575, "J2　1×22 右排（叠插排母 · 长针）", RIGHT, "R")

    # ---- block 1: power protection ----
    b1x0, b1y0, b1x1, b1y1 = 980, 190, 2340, 985
    c.rect(b1x0, b1y0, b1x1, b1y1, fill=(252, 250, 246), outline=(214, 190, 150), w=2, radius=10)
    c.text(b1x0 + 22, b1y0 + 30, "① 电源保护与分配", F(20, True), AMBER, "lm")

    # 5V lane
    lane5 = 300
    c.text(1012, lane5, "5V_IN", F(15, True), RED, "lm")
    c.line(1082, lane5, 1120, lane5, color=RED, w=3)
    comp(c, 1180, lane5, 108, 42, "F1", "PTC 0.5A", color=RED)
    c.line(1234, lane5, 1330, lane5, color=RED, w=3)
    c.circle(1360, lane5, 4, fill=RED)
    c.text(1372, lane5 - 20, "5V_OUT", F(15, True), RED, "lm")
    # branches to GND rail
    gnd5 = 560
    for bx, ref, val in [(1330, "C1", "10µF"), (1440, "C2", "100nF"), (1550, "D1", "SMAJ5.0A")]:
        c.line(bx, lane5, bx, lane5 + 45, color=RED, w=2)
        comp(c, bx, lane5 + 90, 84, 50, ref, val, color=RED)
        c.line(bx, lane5 + 115, bx, gnd5, color=GRAY, w=2)
    c.line(1290, gnd5, 1600, gnd5, color=GRAY, w=3)
    c.text(1610, gnd5, "GND", F(14, True), GRAY, "lm")

    # 3V3 lane
    lane3 = 690
    c.text(1012, lane3, "3V3_IN", F(15, True), ORANGE, "lm")
    c.line(1082, lane3, 1120, lane3, color=ORANGE, w=3)
    comp(c, 1180, lane3, 108, 42, "F2", "PTC 0.5A", color=ORANGE)
    c.line(1234, lane3, 1330, lane3, color=ORANGE, w=3)
    c.circle(1360, lane3, 4, fill=ORANGE)
    c.text(1372, lane3 - 20, "3V3_OUT", F(15, True), ORANGE, "lm")
    gnd3 = 900
    for bx, ref, val in [(1330, "C3", "10µF"), (1440, "C4", "100nF"), (1550, "D2", "SMAJ3.3A")]:
        c.line(bx, lane3, bx, lane3 + 40, color=ORANGE, w=2)
        comp(c, bx, lane3 + 80, 84, 48, ref, val, color=ORANGE)
        c.line(bx, lane3 + 104, bx, gnd3, color=GRAY, w=2)
    c.line(1290, gnd3, 1600, gnd3, color=GRAY, w=3)
    c.text(1610, gnd3, "GND", F(14, True), GRAY, "lm")

    # LED indicator
    c.line(1700, lane3, 1760, lane3, color=ORANGE, w=2)
    comp(c, 1810, lane3, 96, 40, "R1", "1k", color=ORANGE)
    c.line(1858, lane3, 1912, lane3, color=ORANGE, w=2)
    comp(c, 1952, lane3, 76, 40, "LED1", "绿灯", color=ORANGE)
    c.line(1990, lane3, 2020, lane3, color=GRAY, w=2)
    c.line(2020, lane3, 2020, gnd3, color=GRAY, w=2)

    # J3 power output header
    j3x = 2170
    c.text(j3x, 262, "J3 电源输出", F(16, True), DARK, "lm")
    labels3 = [("1", "5V_OUT", RED), ("2", "GND", GRAY), ("3", "3V3_OUT", ORANGE), ("4", "GND", GRAY)]
    for i, (pn, nm, col) in enumerate(labels3):
        yy = 300 + i * 120
        c.rect(j3x, yy - 26, j3x + 60, yy + 26, fill=LIGHT, outline=BOXEDGE, w=1.5, radius=4)
        c.text(j3x + 30, yy, pn, F(14, True), DARK, "mm")
        c.text(j3x + 78, yy, nm, F(15, True), col, "lm")
    c.text(j3x, 300 + 4 * 120 + 6, "2.54 排针 1×4", F(12), GRAY, "lm")

    # ---- block 2: USB ESD ----
    b2x0, b2y0, b2x1, b2y1 = 980, 1010, 2340, 1275
    c.rect(b2x0, b2y0, b2x1, b2y1, fill=(244, 248, 252), outline=(160, 185, 215), w=2, radius=10)
    c.text(b2x0 + 22, b2y0 + 30, "② 原生 USB 接口保护（GPIO19/GPIO20）", F(20, True), BLUE, "lm")
    # D3 USBLC6
    comp(c, 1330, 1140, 150, 66, "D3", "USBLC6-2SC6", color=BLUE, fill=(232, 240, 252), refsz=16, valsz=11)
    # inputs
    c.text(1012, 1105, "GPIO20 (D+)", F(14, True), BLUE, "lm")
    c.line(1170, 1105, 1255, 1105, color=BLUE, w=2)
    c.line(1255, 1105, 1255, 1120, color=BLUE, w=2)
    c.text(1012, 1175, "GPIO19 (D−)", F(14, True), BLUE, "lm")
    c.line(1170, 1175, 1255, 1175, color=BLUE, w=2)
    c.line(1255, 1175, 1255, 1160, color=BLUE, w=2)
    # outputs to J4
    c.line(1405, 1120, 1490, 1120, color=BLUE, w=2)
    c.line(1405, 1160, 1490, 1160, color=BLUE, w=2)
    c.text(1500, 1120, "D+", F(14, True), BLUE, "lm")
    c.text(1500, 1160, "D−", F(14, True), BLUE, "lm")
    c.text(1012, 1230, "VBUS=5V_OUT　GND=GND　（ESD 钳位，防止热插拔打坏原生 USB）", F(13), GRAY, "lm")
    # J4
    j4x = 1650
    c.text(j4x, 1085, "J4 USB 外接（可选）", F(16, True), DARK, "lm")
    labels4 = [("1", "5V_OUT", RED), ("2", "USB_D−", BLUE), ("3", "USB_D+", BLUE), ("4", "GND", GRAY)]
    for i, (pn, nm, col) in enumerate(labels4):
        yy = 1125 + i * 34
        c.rect(j4x, yy - 14, j4x + 40, yy + 14, fill=LIGHT, outline=BOXEDGE, w=1.2, radius=3)
        c.text(j4x + 20, yy, pn, F(12, True), DARK, "mm")
        c.text(j4x + 52, yy, nm, F(13, True), col, "lm")

    # ---- block 3: notes ----
    b3x0, b3y0, b3x1, b3y1 = 120, 1470, 2340, 1720
    c.rect(b3x0, b3y0, b3x1, b3y1, fill=(250, 250, 250), outline=(205, 210, 218), w=2, radius=10)
    c.text(b3x0 + 22, b3y0 + 28, "③ 说明与约束", F(20, True), DARK, "lm")
    notes = [
        "• J1/J2 为 2×22 叠插排母（长针）：开发板直接对插，44 脚 1:1 直通；长针可从底面插入面包板/洞洞板。",
        "• 串口：UART0 调试/下载 = GPIO43(TX)/GPIO44(RX)；UART1 = GPIO17(TX1)/GPIO18(RX1)，图上绿色小字标注。",
        "• 电源：5V0 经 F1(PTC 0.5A) + D1(TVS) + C1/C2 后作为 5V_OUT；3V3 经 F2 + D2 + C3/C4 后作为 3V3_OUT。",
        "• 3V3 由开发板稳压器输出，最大约 1A，外设总电流勿超载；GPIO26–32 为 Flash/PSRAM 专用，未引出。",
        "• GPIO0/45/46 为 strapping，GPIO19/20 为原生 USB：底板上不得加任何上拉/下拉，保持原状。",
        "• 网络名与开发板引脚同名；完整 ADC/TOUCH/UART/SPI 备用功能见 README 引脚表。",
        "• 打样：2 层 / 1.6mm / 1oz，常规工艺；丝印全部放 Top 层，1 脚（方形焊盘）对齐开发板天线端。",
    ]
    for i, s in enumerate(notes):
        c.text(b3x0 + 26, b3y0 + 58 + i * 27, s, F(14), DARK, "lm")

    c.save("ESP32-S3扩展底板_原理图.png")
    print("schematic done")

# ---------------- PCB layout ----------------
def draw_pcb():
    mm = 30.0  # px per mm (final)
    Wmm, Hmm = 57.15, 25.4
    marginx, marginy = 260, 230
    W = int(marginx * 2 + Wmm * mm)
    H = int(marginy * 2 + Hmm * mm + 150)
    c = C(W, H, bg=(238, 241, 245))

    ox, oy = marginx, marginy
    def X(xmm):
        return ox + xmm * mm
    def Y(ymm):
        return oy + ymm * mm

    # board outline
    c.rect(X(0), Y(0), X(Wmm), Y(Hmm), fill=(20, 90, 45), outline=(10, 60, 30), w=2, radius=1.39 * mm)
    # inner solder mask-ish lighter
    c.rect(X(0.2), Y(0.2), X(Wmm - 0.2), Y(Hmm - 0.2), fill=(22, 96, 50), outline=None, radius=1.3 * mm)

    # pin grid
    x0 = 1.905
    pitch = 2.54
    ytop = 1.27
    ybot = Hmm - 1.27
    cols = 22
    pad_r = 0.95  # mm pad radius
    hole_r = 0.45

    # pads (pin 1 = square)
    for i in range(cols):
        xx = x0 + i * pitch
        for yy, nm in ((ytop, LEFT[i][0]), (ybot, RIGHT[i][0])):
            if i == 0 and yy == ytop:
                s = pad_r * mm
                c.rect(X(xx) - s, Y(yy) - s, X(xx) + s, Y(yy) + s, fill=(212, 168, 60),
                       outline=(150, 110, 30), w=1, radius=2)
            else:
                c.circle(X(xx), Y(yy), pad_r * mm, fill=(212, 168, 60), outline=(150, 110, 30), w=1)
            c.circle(X(xx), Y(yy), hole_r * mm, fill=(15, 40, 25))

    # silkscreen pin labels (both rows: labels placed inward, on the green board area)
    uart_suffix = {"GPIO43": "TX", "GPIO44": "RX", "GPIO17": "TX1", "GPIO18": "RX1"}
    for i in range(cols):
        xx = x0 + i * pitch
        for nm, ybase, sign in ((LEFT[i][0], Y(ytop), 1), (RIGHT[i][0], Y(ybot), -1)):
            c.text(X(xx), ybase + sign * 40, nm, F(10, True), (252, 248, 224), "mm")
            if nm in uart_suffix:
                c.text(X(xx), ybase + sign * 57, uart_suffix[nm], F(9, True), (255, 214, 110), "mm")

    # pin1 marker (square pad) label above board
    c.text(X(x0), Y(0) - 34, "1脚", F(13, True), (190, 60, 60), "mm")

    # central components (bottom side) - power protection
    def pcbcomp(cxmm, cymm, wmm, hmm, ref, val):
        x0p, y0p = X(cxmm - wmm / 2), Y(cymm - hmm / 2)
        x1p, y1p = X(cxmm + wmm / 2), Y(cymm + hmm / 2)
        c.rect(x0p, y0p, x1p, y1p, fill=(235, 235, 235), outline=(90, 90, 90), w=1, radius=2)
        c.text((x0p + x1p) / 2, (y0p + y1p) / 2 - 6, ref, F(9, True), (20, 20, 20), "mm")
        c.text((x0p + x1p) / 2, (y0p + y1p) / 2 + 8, val, F(7), (40, 40, 40), "mm")

    # place along center strip
    pcbcomp(9, 8.5, 4.2, 3.0, "F1", "PTC")
    pcbcomp(9, 16.5, 4.2, 3.0, "F2", "PTC")
    pcbcomp(15, 8.5, 3.6, 2.6, "C1", "10uF")
    pcbcomp(15, 16.5, 3.6, 2.6, "C3", "10uF")
    pcbcomp(20, 8.5, 3.2, 2.2, "C2", "100n")
    pcbcomp(20, 16.5, 3.2, 2.2, "C4", "100n")
    pcbcomp(25, 8.5, 3.6, 2.6, "D1", "TVS")
    pcbcomp(25, 16.5, 3.6, 2.6, "D2", "TVS")
    pcbcomp(31, 12.7, 4.2, 2.4, "R1", "1k")
    pcbcomp(36.5, 12.7, 3.0, 3.0, "LED1", "LED")
    pcbcomp(45, 8.5, 5.5, 3.2, "D3", "USBLC6")

    # J3 / J4 pad rows (2.54)
    def padrow(cxmm, cymm, n, prefix, horiz=True):
        for i in range(n):
            if horiz:
                xx = cxmm + (i - (n - 1) / 2) * 2.54
                c.circle(X(xx), Y(cymm), 0.85 * mm, fill=(212, 168, 60), outline=(150, 110, 30), w=1)
                c.circle(X(xx), Y(cymm), 0.4 * mm, fill=(15, 40, 25))
        c.text(X(cxmm), Y(cymm) - 26, prefix, F(11, True), (245, 245, 245), "mm")

    padrow(45, 16.5, 4, "J3")
    padrow(33.5, 6.2, 4, "J4")

    # title silk
    c.text(X(Wmm / 2), Y(Hmm) + 70, "ESP32-S3 扩展底板 v1.0", F(22, True), (30, 40, 55), "mm")
    c.text(X(Wmm / 2), Y(Hmm) + 100, "叠插式引脚转接 · 2×22=44Pin · 57.15×25.4mm · R1.39", F(13), (70, 80, 95), "mm")

    # dimension annotations
    # top width
    c.line(X(0), Y(0) - 95, X(Wmm), Y(0) - 95, color=(60, 70, 90), w=1)
    c.line(X(0), Y(0) - 105, X(0), Y(0) - 85, color=(60, 70, 90), w=1)
    c.line(X(Wmm), Y(0) - 105, X(Wmm), Y(0) - 85, color=(60, 70, 90), w=1)
    c.text(X(Wmm / 2), Y(0) - 110, "57.150 mm", F(14, True), (40, 50, 70), "mm")
    # left height
    c.line(X(0) - 95, Y(0), X(0) - 95, Y(Hmm), color=(60, 70, 90), w=1)
    c.line(X(0) - 105, Y(0), X(0) - 85, Y(0), color=(60, 70, 90), w=1)
    c.line(X(0) - 105, Y(Hmm), X(0) - 85, Y(Hmm), color=(60, 70, 90), w=1)
    c.text(X(0) - 115, Y(Hmm / 2), "25.400 mm", F(14, True), (40, 50, 70), "mm")

    # mounting holes
    for hx in (3.2, Wmm - 3.2):
        c.circle(X(hx), Y(Hmm / 2), 1.1 * mm, fill=(15, 40, 25))
        c.circle(X(hx), Y(Hmm / 2), 0.8 * mm, fill=(238, 241, 245))

    # legend
    lx, ly = 40, H - 120
    c.text(lx, ly, "图例：金黄=焊盘(Φ1.9/孔Φ0.9，方形=1脚)　灰块=底面贴片器件(避免与开发板干涉)", F(12), (50, 60, 80), "lm")
    c.text(lx, ly + 26, "注：排针按 2.54 间距、双排间距 22.86（板宽 25.4 – 两侧各 1.27 边距）布置；以实物开发板实测校正。", F(12), (50, 60, 80), "lm")

    c.save("ESP32-S3扩展底板_PCB布局.png")
    print("pcb done")

if __name__ == "__main__":
    draw_schematic()
    draw_pcb()
