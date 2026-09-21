#include "hierarchicalZBuffer.hpp"
#include <stack>
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"

HierarchicalZBuffer::HierarchicalZBuffer(int width, int height) {
    m_bvh = nullptr;
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

HierarchicalZBuffer::~HierarchicalZBuffer() {
    delete[] m_image;
    delete[] m_pixelQuadNodes;
    delete m_quadTree;
    delete m_bvh;
}

void HierarchicalZBuffer::showInfo() const {
    std::cout << "HierarchicalZBuffer: 四叉树建立时间为" << m_buildTreeTime.count() << "ms, " << "BVH建立时间为" << m_buildBVHTime.count() << "ms, " << "渲染时间为" << m_renderTime.count() << "ms, " << "总时间为" << (m_buildTreeTime + m_buildBVHTime + m_renderTime).count() << "ms" << std::endl;
}

void HierarchicalZBuffer::render() const {
    stbi_flip_vertically_on_write(1);
    std::string modelName = m_modelName;
    stbi_write_png(modelName.insert(0, "results/HierarchicalZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
}

void HierarchicalZBuffer::rasterizeScene(Model& model, Scene& scene) {
    m_modelName = model.getModelName();
    auto start = std::chrono::steady_clock::now();
    buildQuadTree();
    auto end = std::chrono::steady_clock::now();
    m_buildTreeTime = end - start;
    start = std::chrono::steady_clock::now();
    int faceNum = model.getFaceNum();
    model.mvpTransform(scene);
    model.calAxisParams();
    delete m_bvh;
    m_bvh = nullptr;
    std::vector<int> triangles(faceNum);
    for (int i = 0; i < faceNum; i++) {
        triangles[i] = i;
    }
    m_bvh = buildBVH(triangles.data(), 0, faceNum - 1,0 , model);
    end = std::chrono::steady_clock::now();
    m_buildBVHTime = end - start;
    start = std::chrono::steady_clock::now();
    glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
    recursiveRasterizeScene(m_bvh, m_quadTree, model, lightDirection, diffuseColor);
    end = std::chrono::steady_clock::now();
    m_renderTime = end - start;
}

void HierarchicalZBuffer::recursiveRasterizeScene(BVHNode* bvhNode, QuadNode* quadNode, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor) {
    QuadNode* tempQuadNode = quadNode;
    //寻找可以包含BVH节点的最小四叉树节点，同时测试BVH节点是否被四叉树节点遮挡
	while (true) {
		if (tempQuadNode->depth < bvhNode->zmin) {
            return;
		}
        bool isBest = true;
        for (int i = 0; i < 4; i++) {
            if (tempQuadNode->children[i] != nullptr && isInQuadNode(bvhNode, tempQuadNode->children[i])) {
                tempQuadNode = tempQuadNode->children[i];
                isBest = false;
                break;
            }
        }
        if (isBest) {
	        break;
        }
	}
    //如果当前BVH节点未被遮挡，则进行下一步操作
    if (bvhNode->left != nullptr) {
        //当前BVH节点有子节点，则先光栅化子节点
		recursiveRasterizeScene(bvhNode->left, tempQuadNode, model, lightDirection, diffuseColor);
		recursiveRasterizeScene(bvhNode->right, tempQuadNode, model, lightDirection, diffuseColor);
    } else {
        //当前BVH节点没有子节点，说明为叶子节点，则绘制其包含的三角形
		glm::vec3 face[3];
		for (auto it = bvhNode->triangles.begin(); it != bvhNode->triangles.end(); ++it) {
		    model.getFace(*it, face);
		    glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
		    float diffuseIntensity = glm::max(0.f, glm::dot(normal, lightDirection));
		    model.getMVPFace(*it, face);
		    for (int j = 0; j < 3; j++) {
		        face[j].y = static_cast<float>(static_cast<int>(face[j].y));
		    }
		    if (isNeedRasterize(face)) {
		        rasterizeTriangle(face, diffuseColor * diffuseIntensity);
		    }
		}
    }
}

void HierarchicalZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
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
        }
        else {
            xLeft = face[1].x; xRight = face[0].x;
            dxleft = (face[2].x - face[1].x) / faceDiff[1]; dxRight = (face[0].x - face[2].x) / faceDiff[2];
            z = face[1].z;
        }
    }
    else {
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
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
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
            }
            else {
                dxleft = (face[2].x - face[1].x) / faceDiff[1];
                xLeft = face[1].x + dxleft;
                z = face[1].z + (dxleft + static_cast<int>(xLeft) - xLeft) * dzx + dzy;
            }
            ymid--;
        }
        for (int y = ymid; y >= ymin; y--) {
            int ixLeft = static_cast<int>(xLeft), ixRight = static_cast<int>(xRight);
            float tempZ = z;
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
    else if (isTopFlat) {
        //当三角形既是上平底又是下平底时，即三角形是一条平行于y轴的线时，直接从左到右光栅化三角形
        int ixLeft = static_cast<int>(glm::min(face[0].x, glm::min(face[1].x, face[2].x)));
        int ixRihgt = static_cast<int>(glm::max(face[0].x, glm::max(face[1].x, face[2].x)));
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

BVHNode* HierarchicalZBuffer::buildBVH(int* triangles, int left, int right, int axis, Model& model) {
    int faceNum = right - left + 1;
    BVHNode* node = new BVHNode();
    //计算当前构建的BVH节点的包围盒
    float xmin = maxFloat, xmax = minFloat;
    float ymin = maxFloat, ymax = minFloat;
    float zmin = maxFloat, zmax = minFloat;
	for (int i = left; i <= right; i++) {
        node->triangles.push_back(triangles[i]);
        xmin = glm::min(xmin, model.getAxisMinimum(triangles[i], 0));
        xmax = glm::max(xmax, model.getAxisMaximum(triangles[i], 0));
        ymin = glm::min(ymin, model.getAxisMinimum(triangles[i], 1));
        ymax = glm::max(ymax, model.getAxisMaximum(triangles[i], 1));
        zmin = glm::min(zmin, model.getAxisMinimum(triangles[i], 2));
        zmax = glm::max(zmax, model.getAxisMaximum(triangles[i], 2));
	}
    node->xmin = xmin; node->xmax = xmax; node->ymin = ymin; node->ymax = ymax; node->zmin = zmin; node->zmax = zmax;
    //如果当前节点包含三角形超过指定书目，则继续划分，否则成为叶子节点
    if (faceNum <= MAX_TRIANGLE) {
        node->left = nullptr;
        node->right = nullptr;
    }else {
        //此处使用指定轴向的位于中间的三角形的位置作为划分子节点的位置
        int mid = (left + right) / 2;
        partition(triangles, left, right, axis, mid, model);
        //此处使用循环轴向的方法计算以哪个轴向进行划分
        axis = (axis + 1) % 3;
        node->left = buildBVH(triangles, left, mid, axis, model);
        node->right = buildBVH(triangles, mid + 1, right, axis, model);
    }
    return node;
}


void HierarchicalZBuffer::buildQuadTree() {
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
                }
                else {
                    stack.push(node->children[i]);
                }
            }
        }
    }
}

void HierarchicalZBuffer::update(QuadNode* node) {
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
        }
        else {
            break;
        }
    }
}

bool HierarchicalZBuffer::isNeedRasterize(glm::vec3* vertices) const {
    QuadNode* node0 = m_pixelQuadNodes[static_cast<int>(vertices[0].y) * m_width + static_cast<int>(vertices[0].x)];
    QuadNode* node1 = m_pixelQuadNodes[static_cast<int>(vertices[1].y) * m_width + static_cast<int>(vertices[1].x)];
    QuadNode* node2 = m_pixelQuadNodes[static_cast<int>(vertices[2].y) * m_width + static_cast<int>(vertices[2].x)];
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

void HierarchicalZBuffer::partition(int* triangles, int left, int right, int axis, int k, Model& model){
    if (left == right) {
        return;
    }
    //使用快速排序的思想进行划分
    int i = left - 1, j = right + 1;
    float partitionPosition = model.getAxisCenter(triangles[left], axis);
    while (i < j) {
        do {
            i++;
        } while (model.getAxisCenter(triangles[i], axis) < partitionPosition);
        do {
	        j--;
        } while (model.getAxisCenter(triangles[j], axis) > partitionPosition);
        if (i < j) {
            std::swap(triangles[i], triangles[j]);
        }
    }
    if (k <= j) {
        partition(triangles, left, j, axis, k, model);
    } else {
        partition(triangles, j + 1, right, axis, k, model);
    }
}

bool HierarchicalZBuffer::isInQuadNode(BVHNode* bvhNode, QuadNode* quadNode) {
    return quadNode->left - bvhNode->xmin <= EPSILON && quadNode->right - bvhNode->xmax >= -EPSILON &&
        quadNode->bottom - bvhNode->ymin <= EPSILON && quadNode->top - bvhNode->ymax >= -EPSILON;
}