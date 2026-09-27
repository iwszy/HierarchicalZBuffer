#include "basicHierarchicalZBuffer.hpp"
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"
#include "outputPath.hpp"

BasicHierarchicalZBuffer::BasicHierarchicalZBuffer(int width, int height) : m_pyramid(width, height) {
    m_width = width;
    m_height = height;
    m_dirty = false;
    int pixelNum = m_width * m_height;
    m_image = new unsigned char[pixelNum * 4];
    for (int i = 0; i < pixelNum; i++) {
        m_image[i * 4] = 0;
        m_image[i * 4 + 1] = 0;
        m_image[i * 4 + 2] = 0;
        m_image[i * 4 + 3] = 255;
    }
}

BasicHierarchicalZBuffer::~BasicHierarchicalZBuffer() {
    delete[] m_image;
}

void BasicHierarchicalZBuffer::showInfo() const {
    std::cout << "BasicHierarchicalZBuffer: 建金字塔时间为" << m_buildPyramidTime.count() << "ms, " << "渲染时间为" << m_renderTime.count() << "ms, " << "总时间为" << (m_buildPyramidTime + m_renderTime).count() << "ms" << std::endl;
}

void BasicHierarchicalZBuffer::render() const {
    stbi_flip_vertically_on_write(1);
    stbi_write_png(hzb::outputPath("BasicHierarchicalZBuffer", m_modelName).c_str(), m_width, m_height, 4, m_image, 0);
}

void BasicHierarchicalZBuffer::rasterizeScene(Model& model, Scene& scene) {
    m_modelName = model.getModelName();
    //清空颜色缓冲，使同一个对象可以被安全地重复使用（金字塔 / BVH 会在光栅化时重建）
    if (m_dirty) {
	const int pixelNum = m_width * m_height;
	for (int i = 0; i < pixelNum; i++) {
		m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
	}
    }
    m_dirty = true;
    auto start = std::chrono::steady_clock::now();
    m_pyramid.clear();
    auto end = std::chrono::steady_clock::now();
    m_buildPyramidTime = end - start;
    start = std::chrono::steady_clock::now();
    int faceNum = model.getFaceNum();
    model.mvpTransform(scene);
    glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
    const float ambient = scene.getAmbient();
    glm::vec3 face[3];
    for (int i = 0; i < faceNum; i++) {
        model.getFace(i, face);
        glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        float diffuseIntensity = ambient + (1.0f - ambient) * glm::max(0.f, glm::dot(normal, lightDirection));
        glm::vec3 tris[24];
        const int triNum = model.getClippedTriangles(i, scene.getMVP(), scene.getNear(), static_cast<float>(scene.getWidth()), static_cast<float>(scene.getHeight()), tris);
        for (int t = 0; t < triNum; t++) {
            glm::vec3* tri = tris + t * 3;
            //不再把顶点 y 向零取整：那是把几何整体下移最多 1 像素，会让三角形多占一行
            //（有时正好是深度逐位并列的那一行，见 NOTES.md 里黑线的排查）。
            //采样线取整改在光栅化内核里按 ceil/floor 做，覆盖范围与朴素 z-Buffer 的格点判定一致
            if (m_pyramid.isNeedRasterize(tri)) {
                rasterizeTriangle(tri, diffuseColor * diffuseIntensity);
            }
        }
    }
    end = std::chrono::steady_clock::now();
    m_renderTime = end - start;
}

void BasicHierarchicalZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
    rasterizeTriangleScanline(face, color, m_image, m_width, m_height,
        [this](int x, int y, float depth) {
            //层次 z-Buffer：先与金字塔第 0 层（逐像素层）比较，通过后写入并沿父链向上更新
            if (m_pyramid.pixelDepth(x, y) < depth) {
                return false;
            }
            m_pyramid.writePixel(x, y, depth);
            return true;
        });
}
