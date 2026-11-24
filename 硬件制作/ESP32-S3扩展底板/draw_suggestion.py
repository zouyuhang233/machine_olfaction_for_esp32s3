# -*- coding: utf-8 -*-
"""生成「ESP32-S3 扩展底板 · 建议图」（基于用户 U1-U4 + H1-H4 拓扑的修正版）。"""
import os
from PIL import Image, ImageDraw, ImageFont
import draw_board as db

OUT = os.path.dirname(os.path.abspath(__file__))
C = db.C; F = db.F; comp = db.comp
DARK = db.DARK; GRAY = db.GRAY; RED = db.RED; ORANGE = db.ORANGE; TEAL = db.TEAL
GREEN = db.GREEN; BLUE = db.BLUE; PURPLE = db.PURPLE; LIGHT = db.LIGHT
AMBER = db.AMBER; BOXEDGE = db.BOXEDGE; BOXFILL = db.BOXFILL
LEFT = db.LEFT; RIGHT = db.RIGHT; net_color = db.net_color


def show_conn(c, x0, title, pins):
    bw = 250
    x1 = x0 + bw
    ytop, dy = 225, 51
    c.text(x0, 196, title, F(16, True), DARK, "lm")
    c.rect(x0, ytop, x1, ytop + 22 * dy, fill=LIGHT, outline=BOXEDGE, w=2, radius=8)
    for i, (nm, alt) in enumerate(pins):
        yy = ytop + i * dy + dy / 2
        c.text(x1 - 12, yy - 13, str(i + 1), F(11), GRAY, "rm")
        c.text(x0 + 14, yy - 8, nm, F(16, True), DARK, "lm")
        special = {"GPIO43": "TX · U0TXD", "GPIO44": "RX · U0RXD",
                   "GPIO17": "TX1 · U1TXD", "GPIO18": "RX1 · U1RXD"}
        if nm in special:
            c.text(x0 + 14, yy + 13, special[nm], F(9, True), GREEN, "lm")
        elif alt and alt not in ("GND", "5V", "3V3"):
            c.text(x0 + 14, yy + 13, alt, F(9), GRAY, "lm")
        c.line(x1, yy, x1 + 34, yy, color=GREEN, w=2)
        col = net_color(nm)
        c.circle(x1 + 34, yy, 3.2, fill=col)
        c.text(x1 + 44, yy, nm, F(15, True), col, "lm")


def hdr4(c, x, y, ref, netname, color):
    c.text(x, y, ref, F(14, True), DARK, "lm")
    c.text(x + 40, y, netname, F(14, True), color, "lm")
    bx = x + 150
    for k in range(4):
        c.circle(bx, y - 6 + k * 14, 3.4, fill=color, outline=(90, 40, 40))
    c.line(bx, y - 6, bx, y - 6 + 3 * 14, color=color, w=2)
    c.text(bx + 14, y + 18, "1×4", F(10), GRAY, "lm")


def draw():
    c = C(2400, 1800)
    c.grid(40)
    c.text(60, 52, "ESP32-S3 扩展底板 v1.2 · 建议图（叠插引脚转接 + 去耦 + USB-ESD）", F(31, True), DARK, "lm")
    c.text(60, 96, "按你的 U1–U4 + H1–H4 拓扑修正：U1~U4 合并为 1 个 2×22 叠插长针母座（同尺寸）；PTC/TVS 全部省略，仅保留去耦电容；TX/RX 标注；网络名规范化",
           F(15), GRAY, "lm")
    c.line(60, 126, 2340, 126, color=(200, 210, 222), w=2)
    c.text(150, 160, "U1 = 2×22 叠插长针母座（同一器件两排）：顶层插入核心板，底面长针即扩展排针", F(14, True), AMBER, "lm")

    show_conn(c, 130, "U1-A　左排 1–22", LEFT)
    show_conn(c, 560, "U1-B　右排 1–22", RIGHT)

    # ===== block 1: power =====
    x0, y0, x1, y1 = 940, 185, 2350, 970
    c.rect(x0, y0, x1, y1, fill=(252, 250, 246), outline=(214, 190, 150), w=2, radius=10)
    c.text(x0 + 22, y0 + 30, "① 电源去耦与分配", F(20, True), AMBER, "lm")

    # --- 3V3 lane (省略 F1/D1：直连，仅去耦) ---
    ly = 300
    c.text(955, ly, "3V3_IN", F(15, True), ORANGE, "lm")
    c.line(1025, ly, 1690, ly, color=ORANGE, w=3)
    c.circle(1300, ly, 4, fill=ORANGE)
    c.text(1312, ly - 19, "3V3_OUT", F(15, True), ORANGE, "lm")
    c.text(1080, ly + 24, "直连（无 F1/D1，靠核心板稳压器保护）", F(11), GRAY, "lm")
    g3 = 560
    for bx, ref, val in [(1360, "C1", "10µF"), (1460, "C2", "100nF")]:
        c.line(bx, ly, bx, ly + 42, color=ORANGE, w=2)
        comp(c, bx, ly + 86, 84, 50, ref, val, color=ORANGE)
        c.line(bx, ly + 111, bx, g3, color=GRAY, w=2)
    c.line(1330, g3, 1992, g3, color=GRAY, w=3)
    c.text(1700, g3, "GND", F(14, True), GRAY, "lm")
    # LED on 3V3_OUT
    c.line(1690, ly, 1730, ly, color=ORANGE, w=2)
    comp(c, 1785, ly, 96, 40, "R1", "1k", color=ORANGE)
    c.line(1833, ly, 1878, ly, color=ORANGE, w=2)
    comp(c, 1922, ly, 80, 40, "LED1", "绿灯", color=ORANGE)
    c.line(1962, ly, 1990, ly, color=GRAY, w=2)
    c.line(1990, ly, 1990, g3, color=GRAY, w=2)

    # --- 5V lane (省略 F2/D2：直连，仅去耦) ---
    ly2 = 660
    c.text(955, ly2, "5V_IN", F(15, True), RED, "lm")
    c.line(1025, ly2, 1690, ly2, color=RED, w=3)
    c.circle(1300, ly2, 4, fill=RED)
    c.text(1312, ly2 - 19, "5V_OUT", F(15, True), RED, "lm")
    c.text(1080, ly2 + 24, "直连（无 F2/D2）", F(11), GRAY, "lm")
    g5 = 915
    for bx, ref, val in [(1360, "C3", "10µF"), (1460, "C4", "100nF")]:
        c.line(bx, ly2, bx, ly2 + 42, color=RED, w=2)
        comp(c, bx, ly2 + 86, 84, 50, ref, val, color=RED)
        c.line(bx, ly2 + 111, bx, g5, color=GRAY, w=2)
    c.line(1330, g5, 1600, g5, color=GRAY, w=3)
    c.text(1610, g5, "GND", F(14, True), GRAY, "lm")

    # --- headers H1-H4 ---
    hx0, hy0, hx1, hy1 = 2015, 250, 2335, 900
    c.rect(hx0, hy0, hx1, hy1, fill=(246, 246, 250), outline=(170, 175, 195), w=1.5, radius=8)
    c.text(hx0 + 16, hy0 + 26, "H1–H4 电源扩展排针", F(15, True), DARK, "lm")
    hdr4(c, hx0 + 18, 340, "H1", "3V3_OUT", ORANGE)
    hdr4(c, hx0 + 18, 450, "H2", "GND", GRAY)
    hdr4(c, hx0 + 18, 640, "H3", "5V_OUT", RED)
    hdr4(c, hx0 + 18, 750, "H4", "GND", GRAY)

    # ===== block 2: USB ESD =====
    b2x0, b2y0, b2x1, b2y1 = 940, 990, 2350, 1245
    c.rect(b2x0, b2y0, b2x1, b2y1, fill=(244, 248, 252), outline=(160, 185, 215), w=2, radius=10)
    c.text(b2x0 + 22, b2y0 + 30, "② 原生 USB 接口保护（GPIO19 / GPIO20）", F(20, True), BLUE, "lm")
    comp(c, 1330, 1120, 150, 66, "D3", "USBLC6-2SC6", color=BLUE, fill=(232, 240, 252), refsz=16, valsz=11)
    c.text(965, 1085, "GPIO20 (D+)", F(14, True), BLUE, "lm")
    c.line(1120, 1085, 1255, 1085, color=BLUE, w=2); c.line(1255, 1085, 1255, 1100, color=BLUE, w=2)
    c.text(965, 1155, "GPIO19 (D−)", F(14, True), BLUE, "lm")
    c.line(1120, 1155, 1255, 1155, color=BLUE, w=2); c.line(1255, 1155, 1255, 1140, color=BLUE, w=2)
    c.line(1405, 1100, 1490, 1100, color=BLUE, w=2); c.text(1500, 1100, "D+", F(14, True), BLUE, "lm")
    c.line(1405, 1140, 1490, 1140, color=BLUE, w=2); c.text(1500, 1140, "D−", F(14, True), BLUE, "lm")
    c.text(965, 1210, "VBUS=5V_OUT　GND=GND（ESD 钳位，防热插拔打坏原生 USB）", F(13), GRAY, "lm")
    j4x = 1660
    c.text(j4x, 1065, "J4 USB 外接（可选）", F(16, True), DARK, "lm")
    for i, (pn, nm, col) in enumerate([("1", "5V_OUT", RED), ("2", "USB_D−", BLUE), ("3", "USB_D+", BLUE), ("4", "GND", GRAY)]):
        yy = 1105 + i * 34
        c.rect(j4x, yy - 14, j4x + 40, yy + 14, fill=LIGHT, outline=BOXEDGE, w=1.2, radius=3)
        c.text(j4x + 20, yy, pn, F(12, True), DARK, "mm")
        c.text(j4x + 52, yy, nm, F(13, True), col, "lm")

    # ===== notes =====
    nx0, ny0, nx1, ny1 = 100, 1400, 2350, 1780
    c.rect(nx0, ny0, nx1, ny1, fill=(250, 250, 250), outline=(205, 210, 218), w=2, radius=10)
    c.text(nx0 + 22, ny0 + 28, "③ 相对你那版改了什么 / 约束", F(20, True), DARK, "lm")
    notes = [
        "• 机械：U1~U4 合成 1 个 2×22 叠插长针母座（顶接核心板、底长针引出）→ 保持 57.15×25.4 同尺寸；若要独立两排公针，板宽需 ≥30.5mm。",
        "• 电源：3V3 与 5V0 均直连（PTC/TVS 全部省略），各配 C1/C2、C3/C4 去耦 → 3V3_OUT / 5V_OUT；R1+LED1 指示。",
        "• 保护：GPIO19/20 原生 USB 加 D3(USBLC6-2SC6) ESD，可引到 J4；三处均补了你原图缺的接口保护。",
        "• 串口：GPIO43=TX(U0TXD)/GPIO44=RX(U0RXD)；GPIO17=TX1/GPIO18=RX1（绿字标注）。",
        "• 网络名统一 GPIOx / 3V3 / 5V0 / GND；GPIO0/45/46(strapping) 与 GPIO19/20(USB) 不加任何上拉下拉。",
        "• 打样 2 层 / 1.6mm / 1oz；丝印全放 Top；方形焊盘=1 脚，须对齐核心板天线端。",
    ]
    for i, s in enumerate(notes):
        c.text(nx0 + 26, ny0 + 62 + i * 30, s, F(14), DARK, "lm")

    c.save("ESP32-S3扩展底板_建议图.png")
    print("suggestion done")


if __name__ == "__main__":
    draw()
