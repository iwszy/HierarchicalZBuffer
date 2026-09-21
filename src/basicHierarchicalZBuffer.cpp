#include "basicHierarchicalZBuffer.hpp"
#include <stack>
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"

BasicHierarchicalZBuffer::BasicHierarchicalZBuffer(int width, int height) {
    m_quadTree = nullptr;
    m_width = width;
    m_height = height;
    int pixelNum = m_width * m_height;
    m_image = new unsigned char[pixelNum * 4];
    for (int i = 0; i < pixelNum; i++) {
        m_image[i * 4] = 0;
        m_image[i * 4 + 1] = 0;
        m_image[i * 4 + 2] = 0;
        m_image[i * 4 + 3] = 255;
    }
    m_pixelQuadNodes = new QuadNode * [pixelNum];
}

BasicHierarchicalZBuffer::~BasicHierarchicalZBuffer() {
    delete[] m_image;
    delete[] m_pixelQuadNodes;
    delete m_quadTree;
}

void BasicHierarchicalZBuffer::showInfo() const {
    std::cout << "BasicHierarchicalZBuffer: 建树时间为" << m_buildTreeTime.count() << "ms, " << "渲染时间为" << m_renderTime.count() << "ms, " << "总时间为" << (m_buildTreeTime + m_renderTime).count() << "ms" << std::endl;
}

void BasicHierarchicalZBuffer::render() const {
    stbi_flip_vertically_on_write(1);
    std::string modelName = m_modelName;
    stbi_write_png(modelName.insert(0, "results/BasicHierarchicalZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
}

void BasicHierarchicalZBuffer::rasterizeScene(Model& model, Scene& scene) {
    m_modelName = model.getModelName();
    auto start = std::chrono::steady_clock::now();
    buildQuadTree();
    auto end = std::chrono::steady_clock::now();
    m_buildTreeTime = end - start;
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
        if (isNeedRasterize(face)) {
            rasterizeTriangle(face, diffuseColor * diffuseIntensity);
        }
    }
    end = std::chrono::steady_clock::now();
    m_renderTime = end - start;
}

void BasicHierarchicalZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
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
    if (glm::abs(c) < EPSILON) {
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
    if (glm::abs(faceDiff[0]) < EPSILON) {
        isTopFlat = true;
    }
    if (glm::abs(faceDiff[1]) < EPSILON) {
        isBottomFlat = true;
    }
    float xLeft, xRight, dxleft, dxRight, z;
    if (isTopFlat) {
        //如果这个三角形是上平底，则根据顶点face[0],face[1]的左右关系赋值相关参数
        if (face[0].x < face[1].x) {
            xLeft = face[0].x; xRight = face[1].x;
            dxleft = (face[0].x - face[2].x) / faceDiff[2]; dxRight = (face[2].x - face[1].x) / faceDiff[1];
            z = face[0].z;
        }else {
            xLeft = face[1].x; xRight = face[0].x;
            dxleft = (face[2].x - face[1].x) / faceDiff[1]; dxRight = (face[0].x - face[2].x) / faceDiff[2];
            z = face[1].z;
        }
    }else {
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
            if (y < 0 || y >= m_height) {
                //该扫描线在窗口外：不做任何像素操作，只把插值状态推进一行
                xLeft += dxleft;
                xRight += dxRight;
                z += dxleft * dzx + dzy;
                continue;
            }
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
            if (ixLeft < 0) ixLeft = 0;
            if (ixRight >= m_width) ixRight = m_width - 1;
            float tempZ = z + dzx * (ixLeft - xLeft);
            for (int x = ixLeft; x <= ixRight; x++) {
                float depth = glm::abs(tempZ);
                tempZ += dzx;
                int index = y * m_width + x;
                if (m_pixelQuadNodes[index]->depth < depth) {
                    continue;
                }
                m_pixelQuadNodes[index]->depth = depth;
                update(m_pixelQuadNodes[index]->parent);
                m_image[index * 4] = static_cast<unsigned char>(color.r * 255);
                m_image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
                m_image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
            }
	        xLeft += dxleft;
	        xRight += dxRight;
	        z += dxleft * dzx + dzy;
	    }
    }
    if (!isBottomFlat) {
        //如果不是下平底，则执行以下操作，使用扫描线的思想完成下半部分三角形的光栅化
        if (!isTopFlat) {
            //如果不是上平底，则需要根据边face[0]-face[2]与边face[1]-face[2]的左右关系更新相关参数，同时对于非极值点也要将扫描线往下一行
	        if (isLongAtLeft) {
	            dxRight = (face[2].x - face[1].x) / faceDiff[1];
	            xRight = face[1].x + dxRight;
	        }else {
	            dxleft = (face[2].x - face[1].x) / faceDiff[1];
	            xLeft = face[1].x + dxleft;
	            z = face[1].z + (dxleft + static_cast<int>(xLeft) - xLeft) * dzx + dzy;
	        }
            ymid--;
        }
	    for (int y = ymid; y >= ymin; y--) {
            if (y < 0 || y >= m_height) {
                //该扫描线在窗口外：不做任何像素操作，只把插值状态推进一行
                xLeft += dxleft;
                xRight += dxRight;
                z += dxleft * dzx + dzy;
                continue;
            }
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
            float tempZ = z;   //与原实现保持一致，不做子像素修正
            if (ixLeft < 0) {
                tempZ += dzx * (0 - xLeft);   //仅在被裁剪到窗口左边界时补上深度推进量
                ixLeft = 0;
            }
            if (ixRight >= m_width) {
                ixRight = m_width - 1;
            }
            for (int x = ixLeft; x <= ixRight; x++) {
                float depth = glm::abs(tempZ);
                tempZ += dzx;
                int index = y * m_width + x;
                if (m_pixelQuadNodes[index]->depth < depth) {
                    continue;
                }
                m_pixelQuadNodes[index]->depth = depth;
                update(m_pixelQuadNodes[index]->parent);
                m_image[index * 4] = static_cast<unsigned char>(color.r * 255);
                m_image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
                m_image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
            }
            xLeft += dxleft;
            xRight += dxRight;
            z += dxleft * dzx + dzy;
	    }
    }else if (isTopFlat) {
        //当三角形既是上平底又是下平底时，即三角形是一条平行于y轴的线时，直接从左到右光栅化三角形
        if (ymax < 0 || ymax >= m_height) {
            return;   //该退化三角形完全位于窗口外
        }
        int ixLeft = static_cast<int>(glm::min(face[0].x, glm::min(face[1].x, face[2].x)));
        int ixRihgt = static_cast<int>(glm::max(face[0].x, glm::max(face[1].x, face[2].x)));
        if (ixLeft < 0) ixLeft = 0;
        if (ixRihgt >= m_width) ixRihgt = m_width - 1;
        z += dzx * (ixLeft - xLeft);
        for (int x = ixLeft; x <= ixRihgt; x++) {
            float depth = glm::abs(z);
            z += dzx;
            int index = ymax * m_width + x;
            if (m_pixelQuadNodes[index]->depth < depth) {
                continue;
            }
            m_pixelQuadNodes[index]->depth = depth;
            update(m_pixelQuadNodes[index]->parent);
            m_image[index * 4] = static_cast<unsigned char>(color.r * 255);
            m_image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
            m_image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
        }
    }
}

void BasicHierarchicalZBuffer::buildQuadTree() {
    //允许重复调用：先释放上一次构建的树
    delete m_quadTree;
    std::stack<QuadNode*> stack;
    m_quadTree = new QuadNode(nullptr, 0, 0, m_width, 0, m_height);
    stack.push(m_quadTree);
    while (!stack.empty()) {
        auto node = stack.top();
        stack.pop();
        int left = node->left, right = node->right, bottom = node->bottom, top = node->top, level = node->level;
        int wmid = (left + right) / 2, hmid = (bottom + top) / 2;
        node->children[0] = (left == wmid || bottom == hmid) ? nullptr : new QuadNode(node, level + 1, left, wmid, bottom, hmid);
		node->children[1] = (left == wmid || hmid == top) ? nullptr : new QuadNode(node, level + 1, left, wmid, hmid, top);
		node->children[2] = (wmid == right || bottom == hmid) ? nullptr : new QuadNode(node, level + 1, wmid, right, bottom, hmid);
		node->children[3] = (wmid == right || hmid == top) ? nullptr : new QuadNode(node, level + 1, wmid, right, hmid, top);
        for (int i = 0; i < 4; i++) {
            auto child = node->children[i];
            if (child != nullptr) {
                //若子节点不为叶子节点，则压入栈中，否则赋值对应的像素节点
                if (child->left == child->right - 1 && child->bottom == child->top - 1) {
                    m_pixelQuadNodes[child->bottom * m_width + child->left] = node->children[i];
                    child->children[0] = nullptr;
                    child->children[1] = nullptr;
                    child->children[2] = nullptr;
                    child->children[3] = nullptr;
                }else {
					stack.push(node->children[i]);
                }
            }
        }
    }
}

void BasicHierarchicalZBuffer::update(QuadNode* node) {
    float maxDepth;
    QuadNode* tempNode = node;
    while (tempNode != nullptr) {
        maxDepth = minFloat;
        //计算当前节点的4个子节点的最大的深度值
        for (int i = 0; i < 4; i++) {
            if (tempNode->children[i] != nullptr) {
                maxDepth = glm::max(maxDepth, tempNode->children[i]->depth);
            }
        }
        if (tempNode->depth - maxDepth > EPSILON) {
            //如果子节点的最大深度值小于当前节点的最大深度值，则更新此节点以及父节点
            tempNode->depth = maxDepth;
        	tempNode = tempNode->parent;
        }else{
	        break;
        }
    }
}

bool BasicHierarchicalZBuffer::isNeedRasterize(glm::vec3* vertices) const {
    //顶点可能落在窗口外，先夹取到窗口内，避免越界访问 m_pixelQuadNodes
    int vx[3], vy[3];
    for (int i = 0; i < 3; i++) {
        vx[i] = glm::clamp(static_cast<int>(vertices[i].x), 0, m_width - 1);
        vy[i] = glm::clamp(static_cast<int>(vertices[i].y), 0, m_height - 1);
    }
    QuadNode* node0 = m_pixelQuadNodes[vy[0] * m_width + vx[0]];
    QuadNode* node1 = m_pixelQuadNodes[vy[1] * m_width + vx[1]];
    QuadNode* node2 = m_pixelQuadNodes[vy[2] * m_width + vx[2]];
    float z = glm::min(glm::abs(vertices[0].z), glm::min(glm::abs(vertices[1].z), glm::abs(vertices[2].z)));
    //查找可以覆盖三角形的四叉树节点，本质是查找像素节点的公共祖先
	while (node0->level > node1->level) {
        node0 = node0->parent;
    }
    while (node1->level > node0->level) {
        node1 = node1->parent;
    }
    while (node0->level > node2->level) {
        node0 = node0->parent;
    }
    while (node2->level > node0->level) {
        node2 = node2->parent;
    }
    while (node1->level > node2->level) {
        node1 = node1->parent;
    }
    while (node2->level > node1->level) {
        node2 = node2->parent;
    }
    while (!(node0 == node1 && node1 == node2)) {
        node0 = node0->parent;
        node1 = node1->parent;
        node2 = node2->parent;
    }
    //如果三角形最小的深度值小于四叉树节点的深度值，则需要光栅化
    return z < node0->depth;
}