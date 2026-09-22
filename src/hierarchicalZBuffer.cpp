#include "hierarchicalZBuffer.hpp"
#include <stack>
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"

HierarchicalZBuffer::HierarchicalZBuffer(int width, int height) {
    m_bvh = nullptr;
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
    //清空颜色缓冲，使同一个对象可以被安全地重复使用（四叉树/ BVH 会在光栅化时重建）
    if (m_dirty) {
    	const int pixelNum = m_width * m_height;
    	for (int i = 0; i < pixelNum; i++) {
    		m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
    	}
    }
    m_dirty = true;
    auto start = std::chrono::steady_clock::now();
    m_quadTree = buildQuadTree(m_width, m_height, m_pixelQuadNodes, m_quadTree);
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
		    if (isNeedRasterize(m_pixelQuadNodes, m_width, m_height, face)) {
		        rasterizeTriangle(face, diffuseColor * diffuseIntensity);
		    }
		}
    }
}

void HierarchicalZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
    rasterizeTriangleScanline(face, color, m_image, m_width, m_height,
        [this](int index, float depth) {
            //层次 z-Buffer：深度落在四叉树的像素节点上，写完后要逐级向上更新
            QuadNode* node = m_pixelQuadNodes[index];
            if (node->depth < depth) {
                return false;
            }
            node->depth = depth;
            updateQuadTreeDepth(node->parent);
            return true;
        });
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