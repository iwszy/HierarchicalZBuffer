#include "basicZBuffer.hpp"
#include <limits>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.hpp"

BasicZBuffer::BasicZBuffer(int width, int height) {
    m_width = width;
    m_height = height;
    m_dirty = false;
    int pixelNum = m_width * m_height;
    m_zBuffer = new float[pixelNum];
    m_image = new unsigned char[pixelNum * 4];
    float maxFloat = std::numeric_limits <float>::max();
    for (int i = 0; i < pixelNum; i++) {
        m_zBuffer[i] = maxFloat;
        m_image[i * 4] = 0;
        m_image[i * 4 + 1] = 0;
        m_image[i * 4 + 2] = 0;
        m_image[i * 4 + 3] = 255;
    }
}

BasicZBuffer::~BasicZBuffer() {
    delete[] m_zBuffer;
    delete[] m_image;
}

void BasicZBuffer::showInfo() const {
    std::cout << "BasicZBuffer: 渲染时间为" << m_renderTime.count() << "ms" << std::endl;
}

void BasicZBuffer::render() const {
    stbi_flip_vertically_on_write(1);
    std::string modelName = m_modelName;
    stbi_write_png(modelName.insert(0, "results/BasicZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
}

void BasicZBuffer::rasterizeScene(Model& model, Scene& scene) {
    m_modelName = model.getModelName();
    //清空颜色缓冲与 z-Buffer，使同一个对象可以被安全地重复使用
    if (m_dirty) {
    	const int pixelNum = m_width * m_height;
    	for (int i = 0; i < pixelNum; i++) {
    		m_zBuffer[i] = std::numeric_limits<float>::max();
    		m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
    	}
    }
    m_dirty = true;
    auto start = std::chrono::steady_clock::now();
    model.mvpTransform(scene);
    int faceNum = model.getFaceNum();
    glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
    glm::vec3 face[3];
    for (int i = 0; i < faceNum; i++) {
        model.getFace(i, face);
        glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        float diffuseIntensity = glm::max(0.f, glm::dot(normal, lightDirection));
        model.getMVPFace(i, face);
        rasterizeTriangle(face, diffuseColor * diffuseIntensity);
    }
    auto end = std::chrono::steady_clock::now();
    m_renderTime = end - start;
}

void BasicZBuffer::rasterizeTriangle(glm::vec3* vertices, glm::vec3 color) {
    int xmin = std::numeric_limits<int>::max(), xmax = std::numeric_limits<int>::min();
    int ymin = std::numeric_limits<int>::max(), ymax = std::numeric_limits<int>::min();
    //计算三角形的包围盒
    for (int i = 0; i < 3; i++) {
        xmin = glm::min(xmin, static_cast<int>(vertices[i][0]));
        xmax = glm::max(xmax, static_cast<int>(vertices[i][0]));
        ymin = glm::min(ymin, static_cast<int>(vertices[i][1]));
        ymax = glm::max(ymax, static_cast<int>(vertices[i][1]));
    }
    //把包围盒裁剪到渲染窗口内，避免越界写入 m_zBuffer / m_image
    xmin = glm::max(xmin, 0); xmax = glm::min(xmax, m_width - 1);
    ymin = glm::max(ymin, 0); ymax = glm::min(ymax, m_height - 1);
    for (int x = xmin; x <= xmax; x++) {
        for (int y = ymin; y <= ymax; y++) {
            glm::vec3 baryCentricCoordinate = baryCentric(vertices, glm::vec2(x, y));
            //重心坐标全不小于0才能说明该像素在三角形内
            if (baryCentricCoordinate[0] < 0 || baryCentricCoordinate[1] < 0 || baryCentricCoordinate[2] < 0) {
                continue;
            }
            float depth = 0;
            for (int i = 0; i < 3; i++) {
                depth += vertices[i][2] * baryCentricCoordinate[i];
            }
            depth = glm::abs(depth);
            int index = x + y * m_width;
            if (m_zBuffer[index] < depth) {
                continue;
            }
            m_zBuffer[index] = depth;
            m_image[index * 4] = static_cast<unsigned char>(color.r * 255);
            m_image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
            m_image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
        }
    }
}

glm::vec3 BasicZBuffer::baryCentric(glm::vec3* vertices, glm::vec2 point) {
    float w = (vertices[1][0] - vertices[0][0]) * (vertices[2][1] - vertices[0][1]) - (vertices[1][1] - vertices[0][1]) * (vertices[2][0] - vertices[0][0]);
    //三角形面积为0，将像素点当做第1个顶点处理
	if (glm::abs(w) < EPSILON) {
        return { 1, 0, 0 };
    }
	float beta = ((point[0] - vertices[0][0]) * (vertices[2][1] - vertices[0][1]) - (point[1] - vertices[0][1]) * (vertices[2][0] - vertices[0][0])) / w;
    float gamma = ((point[0] - vertices[0][0]) * (vertices[0][1] - vertices[1][1]) - (point[1] - vertices[0][1]) * (vertices[0][0] - vertices[1][0])) / w;
    return { 1 - beta - gamma, beta, gamma };
}