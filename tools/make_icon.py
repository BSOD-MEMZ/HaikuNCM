#!/usr/bin/env python3
"""生成 HaikuNCM 应用图标：Haiku 风格的立体底座 + 网易云蚊香。

Haiku 图标的几条硬规矩（官方 Icon Guidelines + Icon-O-Matic 文档）：
  * 画布按 64x64 网格构图，主要轮廓落在 16x16 网格上，缩小到 16x16 仍要认得出
  * 统一透视：光源在左上，右下方有暗色投影/厚度
  * 深色描边（近黑，不用纯黑），渐变表现体积而不是平涂
  * 颜色取自系统调色板，避免高饱和荧光色

输出：icon/ncm-icon-64.png（预览与 Icon-O-Matic 参考图）+ icon/ncm-icon.svg
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

SIZE = 64
SS = 4                      # 超采样倍数，缩小后边缘才干净
W = SIZE * SS

RED_TOP = (232, 74, 66)
RED_BOTTOM = (188, 40, 38)
SIDE = (146, 26, 24)
OUTLINE = (58, 14, 12)
PAPER = (250, 250, 250)


def rounded(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=radius, fill=fill)


def vertical_gradient(size, top, bottom):
    image = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / max(size - 1, 1)
        image.putpixel((0, y), tuple(
            int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return image.resize((size, size))


def spiral(draw, cx, cy, r_start, r_end, turns, width, color):
    points = []
    steps = 220
    for i in range(steps + 1):
        t = i / steps
        angle = t * turns * 2 * math.pi
        radius = r_start + (r_end - r_start) * t
        points.append((cx + math.cos(angle) * radius,
                       cy + math.sin(angle) * radius))
    draw.line(points, fill=color, width=width, joint="curve")


def build():
    canvas = Image.new("RGBA", (W, W), (0, 0, 0, 0))

    # --- 侧面厚度：先画一个下移的深色圆角块，再把正面盖上去 ---
    side = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    rounded(ImageDraw.Draw(side), (10 * SS, 18 * SS, 54 * SS, 54 * SS), 11 * SS, SIDE)
    canvas.alpha_composite(side)

    # --- 正面：竖向渐变 + 深色描边 ---
    face = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    gradient = vertical_gradient(W, RED_TOP, RED_BOTTOM).convert("RGBA")
    mask = Image.new("L", (W, W), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (10 * SS, 12 * SS, 54 * SS, 48 * SS), radius=11 * SS, fill=255)
    face.paste(gradient, (0, 0), mask)
    canvas.alpha_composite(face)

    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle((10 * SS, 12 * SS, 54 * SS, 48 * SS),
                           radius=11 * SS, outline=OUTLINE, width=int(1.6 * SS))

    # --- 左上高光，模拟塑料外壳 ---
    gloss = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(gloss).ellipse(
        (14 * SS, 13 * SS, 50 * SS, 30 * SS), fill=(255, 255, 255, 46))
    gloss = gloss.filter(ImageFilter.GaussianBlur(2.2 * SS))
    canvas.alpha_composite(gloss)

    # --- 蚊香（网易云标志）：白色的螺旋 ---
    draw = ImageDraw.Draw(canvas)
    spiral(draw, 32 * SS, 30 * SS, 2.4 * SS, 15.2 * SS, 2.6,
           int(3.0 * SS), PAPER)

    return canvas.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "..", "icon")
    os.makedirs(out_dir, exist_ok=True)

    icon = build()
    icon.save(os.path.join(out_dir, "ncm-icon-64.png"))
    icon.resize((32, 32), Image.LANCZOS).save(
        os.path.join(out_dir, "ncm-icon-32.png"))
    icon.resize((16, 16), Image.LANCZOS).save(
        os.path.join(out_dir, "ncm-icon-16.png"))
    print("wrote icon/ncm-icon-{64,32,16}.png")


if __name__ == "__main__":
    main()
