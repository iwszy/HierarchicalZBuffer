#!/usr/bin/env python3
"""生成用于验证层次 Z-Buffer 的高遮挡测试场景。

场景是 M x M x M 个轴对齐立方体按格子排布构成的"体素城市"：从相机看过去，
每条视线都要穿过 M 层立方体，因此 depth complexity 约为 M（正面+背面则约 2M）。
三角形数 = 12 * M^3，例如 M=28 时是 263424，与 Crytek Sponza 的量级相当。

用法:
    python genOcclusionScene.py <M> <输出路径> [n2f|f2n] [CELL]

参数:
    M        每边的格子数
    输出路径  obj 文件路径
    n2f      先输出靠近相机的层（默认）
    f2n      先输出远离相机的层
    CELL     立方体边长占格子的比例，默认 1.06（相邻立方体互相重叠、屏幕上不留缝）。
             取值小于 1 会在立方体之间留下缝隙，此时透过缝隙能看到远处几何，
             深度金字塔的"区域最大深度"会被背景拉高，遮挡剔除率会明显下降 ——
             这本身就是一个值得观察的现象。

注意: 生成的模型会被 Model::loadModel 归一化到 [-1,1]^3（按包围盒中心平移、
按最大坐标绝对值缩放），但由于场景本来就以原点为中心，归一化基本不改变尺寸。
"""
import sys

M = int(sys.argv[1])
outPath = sys.argv[2]
order = sys.argv[3] if len(sys.argv) > 3 else "n2f"
CELL = float(sys.argv[4]) if len(sys.argv) > 4 else 1.06

spacing = 2.0 / M
half = spacing * CELL * 0.5

# 立方体 8 个顶点的局部编号: index = dz*4 + dy*2 + dx，dz/dy/dx 取 0(负) 或 1(正)
# 下面 12 个三角形都按"从外侧看逆时针"给出，保证法线朝外
FACES = [
    (1, 3, 7), (1, 7, 5),   # +X
    (0, 4, 6), (0, 6, 2),   # -X
    (2, 6, 7), (2, 7, 3),   # +Y
    (0, 1, 5), (0, 5, 4),   # -Y
    (4, 5, 7), (4, 7, 6),   # +Z
    (0, 2, 3), (0, 3, 1),   # -Z
]

ks = list(range(M))
if order == "n2f":
    ks = ks[::-1]           # 先输出 z 大的层（离相机近）

base = 0
tri = 0
with open(outPath, "w") as f:
    f.write("# occlusion test scene: %dx%dx%d cubes, order=%s, CELL=%.2f\n" % (M, M, M, order, CELL))
    f.write("# depth complexity along the view direction is about %d\n" % M)
    for k in ks:
        for j in range(M):
            for i in range(M):
                cx = -1.0 + (2 * i + 1) / M
                cy = -1.0 + (2 * j + 1) / M
                cz = -1.0 + (2 * k + 1) / M
                for dz in (-half, half):
                    for dy in (-half, half):
                        for dx in (-half, half):
                            f.write("v %.6f %.6f %.6f\n" % (cx + dx, cy + dy, cz + dz))
                for a, b, c in FACES:
                    f.write("f %d %d %d\n" % (base + a + 1, base + b + 1, base + c + 1))
                base += 8
                tri += 12

print("M=%d order=%s CELL=%.2f  cubes=%d triangles=%d -> %s" % (M, order, CELL, M * M * M, tri, outPath))
