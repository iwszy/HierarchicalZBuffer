#include "basicHierarchicalZBuffer.hpp"
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"

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
    std::string modelName = m_modelName;
    stbi_write_png(modelName.insert(0, "results/BasicHierarchicalZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
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
    glm::vec3 face[3];
    for (int i = 0; i < faceNum; i++) {
        model.getFace(i, face);
        glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        float diffuseIntensity = glm::max(0.f, glm::dot(normal, lightDirection));
        model.getMVPFace(i, face);
	for (int j = 0; j < 3; j++) {
            face[j].y = static_cast<float>(static_cast<int>(face[j].y));
        }
        if (m_pyramid.isNeedRasterize(face)) {
            rasterizeTriangle(face, diffuseColor * diffuseIntensity);
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
