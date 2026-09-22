#ifndef __TRIANGLE_RASTERIZER_HPP__
#define __TRIANGLE_RASTERIZER_HPP__

#include <utility>
#include <glm/glm.hpp>

/*! @brief 用扫描线方式光栅化一个三角形
 *
 *  这是普通 z-Buffer、扫描线 z-Buffer(特化模式)、简单模式层次 z-Buffer、
 *  完整模式层次 z-Buffer 共用的光栅化内核。做法是把三角形按扫描线切成
 *  "上半部分 / 下半部分 / 退化成水平线"三种情况，逐行从左到右遍历像素。
 *
 *  深度的求值方式见文件内 depthAt 的说明：先解出深度平面的 dzx、dzy，
 *  再让每个像素直接代回平面方程，而不是沿扫描线累加状态。
 *
 *  "深度测试 + 写入深度"这一步各算法不同（普通 z-Buffer 只写 z-Buffer；
 *  层次 z-Buffer 还要把四叉树逐级向上更新），所以由调用方通过回调 storeDepth
 *  提供。回调经由模板内联展开，不会引入间接调用开销。
 *
 *  @param[in,out] face: 三角形的 3 个顶点（会被按 y 降序排序），坐标已做 MVP 变换
 *  @param[in] color: 三角形颜色
 *  @param[in,out] image: 渲染窗口的 RGBA 缓冲
 *  @param[in] width: 渲染窗口宽度
 *  @param[in] height: 渲染窗口高度
 *  @param[in] storeDepth: 回调，签名 bool(int x, int y, float depth)。
 *                         返回 true 表示该像素通过深度测试、深度已写入，可以继续画颜色；
 *                         返回 false 表示被已有几何遮挡，跳过该像素
 */
template <typename StoreDepth>
inline void rasterizeTriangleScanline(glm::vec3* face, glm::vec3 color,
                                      unsigned char* image, int width, int height,
                                      StoreDepth storeDepth)
{
    //与原各算法中的 EPSILON 宏取值一致
    constexpr double kEpsilon = 1e-5;

    //深度一律取 -z，而不是 |z|。
    //本工程的投影（m_near/m_far 都取负值、配合行向量约定）把近平面映到
    //z_ndc = +1、远平面映到 -1，是一个"反向 Z"映射：z_ndc 随距离【单调递减】。
    //而 z-Buffer 的常规约定是"深度越小越近、z-test 用 <"，要求深度随距离【单调递增】，
    //所以取 -z —— 它把整个视锥线性翻转回来，取值落在 [-1, +1]，与常规深度缓冲一致，
    //而且数值上就是同一个 float 取负，没有任何额外开销。
    //反观 |z|：它在 z_ndc 的过零点处把深度次序折叠了一次，该点在本工程中位于
    //相机前方约 0.6 处 —— 那是视锥【内部】，而不是近平面。任何越过该点的三角形
    //前后关系都会整体反转，离相机更远的那部分反而会被判为更近。

    //将三个顶点根据y的大小降序排序
    if (face[0].y < face[1].y) {
        std::swap(face[0], face[1]);
    }
    if (face[0].y < face[2].y) {
        std::swap(face[0], face[2]);
    }
    if (face[1].y < face[2].y) {
        std::swap(face[1], face[2]);
    }
    float faceDiff[3];
    for (int i = 0; i < 3; i++) {
        faceDiff[i] = face[i].y - face[(i + 1) % 3].y;
    }
    float a = faceDiff[0] * (face[0].z - face[2].z) - (face[1].z - face[0].z) * faceDiff[2];
    float b = (face[1].z - face[0].z) * (face[2].x - face[0].x) - (face[1].x - face[0].x) * (face[2].z - face[0].z);
    float c = (face[1].x - face[0].x) * faceDiff[2] + faceDiff[0] * (face[2].x - face[0].x);
    float dzx = -a / c, dzy = b / c;
    if (glm::abs(c) < kEpsilon) {
        //与经典扫描线算法建表的思路相同，当三角形是一条线时执行以下操作
        int minXIndex = 0, maxXIndex = 0;
        float minX = face[0].x, maxX = minX;
        for (int j = 1; j < 3; j++) {
            if (minX > face[j].x) {
                minX = face[j].x;
                minXIndex = j;
            }
            if (maxX < face[j].x) {
                maxX = face[j].x;
                maxXIndex = j;
            }
        }
        dzx = (face[maxXIndex].z - face[minXIndex].z) / (maxX - minX);
        dzy = (face[2].z - face[0].z) / faceDiff[2];
    }
    bool isTopFlat = false, isBottomFlat = false, isLongAtLeft = false;
    if (glm::abs(faceDiff[0]) < kEpsilon) {
        isTopFlat = true;
    }
    if (glm::abs(faceDiff[1]) < kEpsilon) {
        isBottomFlat = true;
    }
    //解出的 dzx、dzy 描述的是同一个深度平面：
    //    z(x, y) = face[0].z + (face[0].y - y) * dzy + (x - face[0].x) * dzx
    //每个像素都直接代回这个式子求值。原来的写法是沿扫描线逐行累加
    //（z += dxleft * dzx + dzy），而这两项经常是一对量级相同、符号相反的
    //大数（三角形越窄长越极端，实测单步可以到 ±13），它们相减本身就带误差，
    //再乘上成百上千行，误差就积累成了肉眼可见的深度错误。
    //退化成长条时 faceDiff 可能全为 0、dzy 是 inf，用 (dy == 0) 短路绕开它。
    const float refX = face[0].x, refY = face[0].y, refZ = face[0].z;
    auto depthAt = [dzx, dzy, refX, refY, refZ](float x, float y) {
        const float dy = refY - y;
        return -(refZ + (dy == 0.0f ? 0.0f : dy * dzy) + (x - refX) * dzx);
    };
    //像素范围是由浮点边界向零截断（static_cast<int>）得到的，最左/最右那个像素
    //可能落在扫描行的真实跨度之外最多 1 像素。对普通三角形这点外推无所谓，
    //但三角形越窄长 |dzx| 越大（实测可以到几十），外推出去深度会被甩到
    //[-1, 1] 之外（实测出现过 +18.9 和 -37.2）。
    //把求值位置夹回跨度之内即可：对跨度内的像素夹取是恒等变换，取值完全不变。
    auto clampToSpan = [](float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    };
    float xLeft, xRight, dxleft, dxRight, z;
    if (isTopFlat) {
        //如果这个三角形是上平底，则根据顶点face[0],face[1]的左右关系赋值相关参数
        if (face[0].x < face[1].x) {
            xLeft = face[0].x; xRight = face[1].x;
            dxleft = (face[0].x - face[2].x) / faceDiff[2]; dxRight = (face[2].x - face[1].x) / faceDiff[1];
            z = face[0].z;
        } else {
            xLeft = face[1].x; xRight = face[0].x;
            dxleft = (face[2].x - face[1].x) / faceDiff[1]; dxRight = (face[0].x - face[2].x) / faceDiff[2];
            z = face[1].z;
        }
    } else {
        //如果这个三角形不是上平底，则根据边face[0]-face[1],face[0]-face[2]的左右关系赋值相关参数
        xLeft = face[0].x; xRight = face[0].x;
        dxleft = (face[1].x - face[0].x) / faceDiff[0]; dxRight = (face[0].x - face[2].x) / faceDiff[2];
        if (dxleft > dxRight) {
            std::swap(dxleft, dxRight);
            isLongAtLeft = true;
        }
        z = face[0].z;
    }
    int ymax = static_cast<int>(face[0].y), ymid = static_cast<int>(face[1].y), ymin = static_cast<int>(face[2].y);
    if (!isTopFlat) {
        //如果不是上平底，则执行以下循环，使用扫描线的思想完成上半部分三角形的光栅化
        for (int y = ymax; y >= ymid; y--) {
            if (y < 0 || y >= height) {
                //该扫描线在窗口外：不做任何像素操作，只把边界推进一行
                xLeft += dxleft;
                xRight += dxRight;
                continue;
            }
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
            if (ixLeft < 0) ixLeft = 0;
            if (ixRight >= width) ixRight = width - 1;
            for (int x = ixLeft; x <= ixRight; x++) {
                const float depth = depthAt(clampToSpan(static_cast<float>(x), xLeft, xRight), static_cast<float>(y));
                if (!storeDepth(x, y, depth)) {
                    continue;
                }
                const int index = y * width + x;
                image[index * 4] = static_cast<unsigned char>(color.r * 255);
                image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
                image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
            }
            xLeft += dxleft;
            xRight += dxRight;
        }
    }
    if (!isBottomFlat) {
        //如果不是下平底，则执行以下操作，使用扫描线的思想完成下半部分三角形的光栅化
        if (!isTopFlat) {
            //如果不是上平底，则需要根据边face[0]-face[2]与边face[1]-face[2]的左右关系更新相关参数，同时对于非极值点也要将扫描线往下一行
            if (isLongAtLeft) {
                dxRight = (face[2].x - face[1].x) / faceDiff[1];
                xRight = face[1].x + dxRight;
            } else {
                dxleft = (face[2].x - face[1].x) / faceDiff[1];
                xLeft = face[1].x + dxleft;
            }
            ymid--;
        }
        for (int y = ymid; y >= ymin; y--) {
            if (y < 0 || y >= height) {
                //该扫描线在窗口外：不做任何像素操作，只把边界推进一行
                xLeft += dxleft;
                xRight += dxRight;
                continue;
            }
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
            if (ixLeft < 0) ixLeft = 0;
            if (ixRight >= width) ixRight = width - 1;
            for (int x = ixLeft; x <= ixRight; x++) {
                const float depth = depthAt(clampToSpan(static_cast<float>(x), xLeft, xRight), static_cast<float>(y));
                if (!storeDepth(x, y, depth)) {
                    continue;
                }
                const int index = y * width + x;
                image[index * 4] = static_cast<unsigned char>(color.r * 255);
                image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
                image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
            }
            xLeft += dxleft;
            xRight += dxRight;
        }
    } else if (isTopFlat) {
        //当三角形既是上平底又是下平底时，即三角形退化成一条平行于 x 轴的线时，直接从左到右光栅化
        //此时 faceDiff 全为 0、平面方程退化，所以沿用 (xLeft, z) 这个参考点沿 x 线性求值
        if (ymax < 0 || ymax >= height) {
            return;   //该退化三角形完全位于窗口外
        }
        const float lineMinX = glm::min(face[0].x, glm::min(face[1].x, face[2].x));
        const float lineMaxX = glm::max(face[0].x, glm::max(face[1].x, face[2].x));
        int ixLeft = static_cast<int>(lineMinX);
        int ixRihgt = static_cast<int>(lineMaxX);
        if (ixLeft < 0) ixLeft = 0;
        if (ixRihgt >= width) ixRihgt = width - 1;
        for (int x = ixLeft; x <= ixRihgt; x++) {
            const float depth = -(z + dzx * (clampToSpan(static_cast<float>(x), lineMinX, lineMaxX) - xLeft));
            if (!storeDepth(x, ymax, depth)) {
                continue;
            }
            const int index = ymax * width + x;
            image[index * 4] = static_cast<unsigned char>(color.r * 255);
            image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
            image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
        }
    }
}

#endif // !__TRIANGLE_RASTERIZER_HPP__
