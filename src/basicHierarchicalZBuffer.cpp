#include "basicHierarchicalZBuffer.hpp"
#include <stack>
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"

BasicHierarchicalZBuffer::BasicHierarchicalZBuffer(int width, int height) {
    m_quadTree = nullptr;
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
    //清空颜色缓冲，使同一个对象可以被安全地重复使用（四叉树/ BVH 会在光栅化时重建）
    if (m_dirty) {
    	const int pixelNum = m_width * m_height;
    	for (int i = 0; i < pixelNum; i++) {
    		m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
    	}
    }
    m_dirty = true;
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
    rasterizeTriangleScanline(face, color, m_image, m_width, m_height,
        [this](int index, float depth) {
            //层次 z-Buffer：深度落在四叉树的像素节点上，写完后要逐级向上更新
            QuadNode* node = m_pixelQuadNodes[index];
            if (node->depth < depth) {
                return false;
            }
            node->depth = depth;
            update(node->parent);
            return true;
        });
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