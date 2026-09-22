#include "hierarchicalZBuffer.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"

HierarchicalZBuffer::HierarchicalZBuffer(int width, int height) : m_pyramid(width, height) {
    m_structure = SceneStructure::BVH;
    m_bvh = nullptr;
    m_octree = nullptr;
    m_bvhFaceNum = -1;
    m_bvhStructure = SceneStructure::BVH;
    m_width = width;
    m_height = height;
    m_dirty = false;
    m_mvp = glm::mat4(1.0f);
    int pixelNum = m_width * m_height;
    m_image = new unsigned char[pixelNum * 4];
    for (int i = 0; i < pixelNum; i++) {
        m_image[i * 4] = 0;
        m_image[i * 4 + 1] = 0;
        m_image[i * 4 + 2] = 0;
        m_image[i * 4 + 3] = 255;
    }
}

HierarchicalZBuffer::~HierarchicalZBuffer() {
    delete[] m_image;
    delete m_bvh;
    delete m_octree;
}

void HierarchicalZBuffer::setSceneStructure(SceneStructure structure) {
    m_structure = structure;
}

void HierarchicalZBuffer::showInfo() const {
    std::cout << (m_structure == SceneStructure::Octree ? "OctreeHierarchicalZBuffer: " : "HierarchicalZBuffer: ")
        << "金字塔构建时间为" << m_buildPyramidTime.count() << "ms, "
        << (m_structure == SceneStructure::Octree ? "八叉树建立时间为" : "BVH建立时间为") << m_buildBVHTime.count() << "ms, "
        << "渲染时间为" << m_renderTime.count() << "ms, "
        << "总时间为" << (m_buildPyramidTime + m_buildBVHTime + m_renderTime).count() << "ms" << std::endl;
}

void HierarchicalZBuffer::render() const {
    stbi_flip_vertically_on_write(1);
    std::string modelName = m_modelName;
    const char* prefix = (m_structure == SceneStructure::Octree)
        ? "results/OctreeHierarchicalZBuffer_"
        : "results/HierarchicalZBuffer_";
    stbi_write_png(modelName.insert(0, prefix).append(".png").c_str(), m_width, m_height, 4, m_image, 0);
}

HierarchicalZBuffer::ScreenBounds HierarchicalZBuffer::projectBounds(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, const glm::mat4& mvp) {
    ScreenBounds sb;
    sb.xmin = sb.ymin = std::numeric_limits<float>::max();
    sb.xmax = sb.ymax = std::numeric_limits<float>::lowest();
    sb.zmin = std::numeric_limits<float>::max();
    sb.valid = true;
    for (int c = 0; c < 8; c++) {
        const glm::vec3 corner((c & 1) ? xmax : xmin,
                               (c & 2) ? ymax : ymin,
                               (c & 4) ? zmax : zmin);
        const glm::vec4 h = glm::vec4(corner, 1.0f) * mvp;
        //注意：本工程的投影用的是行向量约定(v * MVP)，齐次分量 w 就是相机空间的 z；
        //相机朝 -z 方向看，所以可见几何的 w 恒为负值，只有 w >= 0 才是"跑到相机后面"
        if (h.w >= -1e-4f) {
            //顶点跑到相机后面（或恰好在相机平面上）：投影结果不可靠，本帧放弃剔除
            sb.valid = false;
            sb.xmin = sb.ymin = std::numeric_limits<float>::lowest();
            sb.xmax = sb.ymax = std::numeric_limits<float>::max();
            sb.zmin = 0.0f;
            return sb;
        }
        const glm::vec3 s(h.x / h.w, h.y / h.w, h.z / h.w);
        sb.xmin = std::min(sb.xmin, s.x);
        sb.xmax = std::max(sb.xmax, s.x);
        sb.ymin = std::min(sb.ymin, s.y);
        sb.ymax = std::max(sb.ymax, s.y);
        sb.zmin = std::min(sb.zmin, std::abs(s.z));
    }
    return sb;
}

bool HierarchicalZBuffer::descendPyramid(const ScreenBounds& sb, int& level, int& qx, int& qy) const {
    //从当前层次开始：先做遮挡判断，能通过就再往细一层下探，直到下不去或者已经是最细一层
    while (true) {
        if (sb.valid && m_pyramid.depthAt(level, qx, qy) < sb.zmin) {
            return false;
        }
        if (level == 0) {
            break;
        }
        const int childLevel = level - 1;
        bool isBest = true;
        for (int i = 0; i < 4; i++) {
            const int cx = (qx << 1) + (i & 1);
            const int cy = (qy << 1) + (i >> 1);
            if (cx >= m_pyramid.levelWidth(childLevel) || cy >= m_pyramid.levelHeight(childLevel)) {
                continue;
            }
            if (m_pyramid.nodeContains(childLevel, cx, cy, sb.xmin, sb.xmax, sb.ymin, sb.ymax)) {
                level = childLevel;
                qx = cx;
                qy = cy;
                isBest = false;
                break;
            }
        }
        if (isBest) {
            break;
        }
    }
    return true;
}

void HierarchicalZBuffer::rasterizeTriangles(const std::vector<int>& triangles, Model& model, const glm::vec3& lightDirection, const glm::vec3& diffuseColor) {
    glm::vec3 face[3];
    for (auto it = triangles.begin(); it != triangles.end(); ++it) {
        model.getFace(*it, face);
        glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        float diffuseIntensity = m_ambient + (1.0f - m_ambient) * glm::max(0.f, glm::dot(normal, lightDirection));
        model.getMVPFace(*it, face);
        for (int j = 0; j < 3; j++) {
            face[j].y = static_cast<float>(static_cast<int>(face[j].y));
        }
        if (m_pyramid.isNeedRasterize(face)) {
            rasterizeTriangle(face, diffuseColor * diffuseIntensity);
        }
    }
}

void HierarchicalZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
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

void HierarchicalZBuffer::recursiveRasterizeBVH(BVHNode* bvhNode, const ScreenBounds& sb, int level, int qx, int qy, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor) {
    if (!descendPyramid(sb, level, qx, qy)) {
        return;
    }
    if (bvhNode->left != nullptr) {
        //当前BVH节点有子节点：先投影两个孩子，再按"由近到远"的顺序递归。
        //注意 BVH 的两个子节点在深度上是互相重叠的，所以这个顺序只是弱排序
        const ScreenBounds lsb = projectBounds(bvhNode->left->xmin, bvhNode->left->xmax, bvhNode->left->ymin, bvhNode->left->ymax, bvhNode->left->zmin, bvhNode->left->zmax, m_mvp);
        const ScreenBounds rsb = projectBounds(bvhNode->right->xmin, bvhNode->right->xmax, bvhNode->right->ymin, bvhNode->right->ymax, bvhNode->right->zmin, bvhNode->right->zmax, m_mvp);
        if (lsb.zmin <= rsb.zmin) {
            recursiveRasterizeBVH(bvhNode->left, lsb, level, qx, qy, model, lightDirection, diffuseColor);
            recursiveRasterizeBVH(bvhNode->right, rsb, level, qx, qy, model, lightDirection, diffuseColor);
        } else {
            recursiveRasterizeBVH(bvhNode->right, rsb, level, qx, qy, model, lightDirection, diffuseColor);
            recursiveRasterizeBVH(bvhNode->left, lsb, level, qx, qy, model, lightDirection, diffuseColor);
        }
    } else {
        //当前BVH节点没有子节点，说明为叶子节点，则绘制其包含的三角形
        rasterizeTriangles(bvhNode->triangles, model, lightDirection, diffuseColor);
    }
}

void HierarchicalZBuffer::recursiveRasterizeOctree(OctreeNode* octNode, const ScreenBounds& sb, int level, int qx, int qy, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor) {
    if (!descendPyramid(sb, level, qx, qy)) {
        return;
    }
    if (octNode->isLeaf()) {
        rasterizeTriangles(octNode->triangles, model, lightDirection, diffuseColor);
        return;
    }
    //把非空的子节点投影到屏幕空间，再按"由近到远"插入排序。
    //这里和 BVH 的关键区别：八叉树的 8 个子节点是互不重叠的空间单元，
    //所以按最小深度排序得到的是真正有效的由近到远顺序，近处的几何会先把
    //深度金字塔填满，后面的兄弟子树才可能被整棵剔除。
    OctreeNode* order[8];
    ScreenBounds bounds[8];
    int n = 0;
    for (int i = 0; i < 8; i++) {
        OctreeNode* child = octNode->children[i];
        if (child == nullptr) {
            continue;
        }
        const ScreenBounds cb = projectBounds(child->bxmin, child->bxmax, child->bymin, child->bymax, child->bzmin, child->bzmax, m_mvp);
        int j = n++;
        while (j > 0 && bounds[j - 1].zmin > cb.zmin) {
            bounds[j] = bounds[j - 1];
            order[j] = order[j - 1];
            j--;
        }
        bounds[j] = cb;
        order[j] = child;
    }
    for (int i = 0; i < n; i++) {
        recursiveRasterizeOctree(order[i], bounds[i], level, qx, qy, model, lightDirection, diffuseColor);
    }
}

void HierarchicalZBuffer::buildScene(Model& model) {
    model.calAxisParamsObject();
    delete m_bvh;
    m_bvh = nullptr;
    delete m_octree;
    m_octree = nullptr;
    const int faceNum = model.getFaceNum();
    m_bvhFaceNum = faceNum;
    m_bvhModelName = model.getModelName();
    m_bvhStructure = m_structure;
    if (faceNum <= 0) {
        return;
    }
    m_triangles.resize(faceNum);
    for (int i = 0; i < faceNum; i++) {
        m_triangles[i] = i;
    }
    if (m_structure == SceneStructure::Octree) {
        //模型在 loadModel 里已经被归一化到 [-1,1]^3，正好可以作为八叉树的根单元
        m_octree = buildOctree(m_triangles.data(), faceNum, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 0, model);
    } else {
        m_bvh = buildBVH(m_triangles.data(), 0, faceNum - 1, 0, model);
    }
}

void HierarchicalZBuffer::rasterizeScene(Model& model, Scene& scene) {
    m_modelName = model.getModelName();
    //清空颜色缓冲，使同一个对象可以被安全地重复使用
    if (m_dirty) {
		const int pixelNum = m_width * m_height;
		for (int i = 0; i < pixelNum; i++) {
			m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
		}
    }
    m_dirty = true;

    //每帧只需要重建 Z-max 金字塔
    auto start = std::chrono::steady_clock::now();
    m_pyramid.clear();
    auto end = std::chrono::steady_clock::now();
    m_buildPyramidTime = end - start;

    const int faceNum = model.getFaceNum();
    model.mvpTransform(scene);      //光栅化用的是屏幕坐标，每帧都要重新变换
    m_mvp = scene.getMVP();

    //加速结构建在模型空间、与相机无关，同一个模型只需要构建一次，之后每帧直接复用
    start = std::chrono::steady_clock::now();
    if ((m_bvh == nullptr && m_octree == nullptr) || m_bvhFaceNum != faceNum
        || m_bvhModelName != m_modelName || m_bvhStructure != m_structure) {
        buildScene(model);
    }
    end = std::chrono::steady_clock::now();
    m_buildBVHTime = end - start;

    if (m_structure == SceneStructure::Octree) {
        if (m_octree == nullptr) {
            m_renderTime = std::chrono::duration<double, std::milli>(0);
            return;
        }
    } else if (m_bvh == nullptr) {
        m_renderTime = std::chrono::duration<double, std::milli>(0);
        return;
    }

    start = std::chrono::steady_clock::now();
    glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
    m_ambient = scene.getAmbient();
    if (m_structure == SceneStructure::Octree) {
        const ScreenBounds sb = projectBounds(m_octree->bxmin, m_octree->bxmax, m_octree->bymin, m_octree->bymax, m_octree->bzmin, m_octree->bzmax, m_mvp);
        recursiveRasterizeOctree(m_octree, sb, m_pyramid.rootLevel(), 0, 0, model, lightDirection, diffuseColor);
    } else {
        const ScreenBounds sb = projectBounds(m_bvh->xmin, m_bvh->xmax, m_bvh->ymin, m_bvh->ymax, m_bvh->zmin, m_bvh->zmax, m_mvp);
        recursiveRasterizeBVH(m_bvh, sb, m_pyramid.rootLevel(), 0, 0, model, lightDirection, diffuseColor);
    }
    end = std::chrono::steady_clock::now();
    m_renderTime = end - start;
}

BVHNode* HierarchicalZBuffer::buildBVH(int* triangles, int left, int right, int axis, Model& model) {
    int faceNum = right - left + 1;
    BVHNode* node = new BVHNode();
    //计算当前构建的BVH节点的包围盒（模型空间）
    constexpr float kFloatMax = std::numeric_limits<float>::max();
    constexpr float kFloatLowest = std::numeric_limits<float>::lowest();
    float xmin = kFloatMax, xmax = kFloatLowest;
    float ymin = kFloatMax, ymax = kFloatLowest;
    float zmin = kFloatMax, zmax = kFloatLowest;
	for (int i = left; i <= right; i++) {
        node->triangles.push_back(triangles[i]);
        xmin = glm::min(xmin, model.getObjectAxisMinimum(triangles[i], 0));
        xmax = glm::max(xmax, model.getObjectAxisMaximum(triangles[i], 0));
        ymin = glm::min(ymin, model.getObjectAxisMinimum(triangles[i], 1));
        ymax = glm::max(ymax, model.getObjectAxisMaximum(triangles[i], 1));
        //z 必须用有符号版本：getObjectAxisMinimum/Maximum(2) 返回的是 |z|，
        //算出来的包围盒投影到屏幕空间后位置和深度都是错的
        zmin = glm::min(zmin, model.getObjectZMin(triangles[i]));
        zmax = glm::max(zmax, model.getObjectZMax(triangles[i]));
	}
    node->xmin = xmin; node->xmax = xmax; node->ymin = ymin; node->ymax = ymax; node->zmin = zmin; node->zmax = zmax;
    //如果当前节点包含三角形超过指定数目，则继续划分，否则成为叶子节点
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

OctreeNode* HierarchicalZBuffer::buildOctree(int* triangles, int count, float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, int depth, Model& model) {
    OctreeNode* node = new OctreeNode();
    node->xmin = xmin; node->xmax = xmax;
    node->ymin = ymin; node->ymax = ymax;
    node->zmin = zmin; node->zmax = zmax;
    //节点内三角形的紧包围盒：三角形是按重心分到八分体里的，可能略微越界，
    //遮挡判断必须用这个紧包围盒才安全
    constexpr float kFloatMax = std::numeric_limits<float>::max();
    constexpr float kFloatLowest = std::numeric_limits<float>::lowest();
    float bxmin = kFloatMax, bxmax = kFloatLowest;
    float bymin = kFloatMax, bymax = kFloatLowest;
    float bzmin = kFloatMax, bzmax = kFloatLowest;
    for (int i = 0; i < count; i++) {
        bxmin = glm::min(bxmin, model.getObjectAxisMinimum(triangles[i], 0));
        bxmax = glm::max(bxmax, model.getObjectAxisMaximum(triangles[i], 0));
        bymin = glm::min(bymin, model.getObjectAxisMinimum(triangles[i], 1));
        bymax = glm::max(bymax, model.getObjectAxisMaximum(triangles[i], 1));
        bzmin = glm::min(bzmin, model.getObjectZMin(triangles[i]));
        bzmax = glm::max(bzmax, model.getObjectZMax(triangles[i]));
    }
    node->bxmin = bxmin; node->bxmax = bxmax;
    node->bymin = bymin; node->bymax = bymax;
    node->bzmin = bzmin; node->bzmax = bzmax;
    if (count <= MAX_TRIANGLE || depth >= MAX_OCTREE_DEPTH) {
        node->leaf = true;
        node->triangles.assign(triangles, triangles + count);
        return node;
    }
    //按三角形重心落在哪个八分体，把三角形原地分成 8 组（计数排序）
    const float mx = (xmin + xmax) * 0.5f;
    const float my = (ymin + ymax) * 0.5f;
    const float mz = (zmin + zmax) * 0.5f;
    int counts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    m_octantIdx.resize(count);
    for (int i = 0; i < count; i++) {
        //必须用【有符号】的真实中心：z 轴的 getObjectAxisCenter 存的是 |z| 的中心，
        //恒为非负，会让所有三角形都落进 +z 一侧
        const int o = (model.getObjectCentroid(triangles[i], 0) >= mx ? 1 : 0)
                    | (model.getObjectCentroid(triangles[i], 1) >= my ? 2 : 0)
                    | (model.getObjectCentroid(triangles[i], 2) >= mz ? 4 : 0);
        m_octantIdx[i] = o;
        counts[o]++;
    }
    int start[8];
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        start[i] = sum;
        sum += counts[i];
    }
    m_octantTmp.resize(count);
    int cursor[8];
    for (int i = 0; i < 8; i++) {
        cursor[i] = start[i];
    }
    for (int i = 0; i < count; i++) {
        m_octantTmp[cursor[m_octantIdx[i]]++] = triangles[i];
    }
    for (int i = 0; i < count; i++) {
        triangles[i] = m_octantTmp[i];
    }
    //递归构建 8 个子节点
    for (int i = 0; i < 8; i++) {
        if (counts[i] == 0) {
            continue;
        }
        const float cx0 = (i & 1) ? mx : xmin, cx1 = (i & 1) ? xmax : mx;
        const float cy0 = (i & 2) ? my : ymin, cy1 = (i & 2) ? ymax : my;
        const float cz0 = (i & 4) ? mz : zmin, cz1 = (i & 4) ? zmax : mz;
        node->children[i] = buildOctree(triangles + start[i], counts[i], cx0, cx1, cy0, cy1, cz0, cz1, depth + 1, model);
    }
    return node;
}

void HierarchicalZBuffer::partition(int* triangles, int left, int right, int axis, int k, Model& model){
    if (left == right) {
        return;
    }
    //使用快速排序的思想进行划分
    int i = left - 1, j = right + 1;
    float partitionPosition = model.getObjectAxisCenter(triangles[left], axis);
    while (i < j) {
        do {
            i++;
        } while (model.getObjectAxisCenter(triangles[i], axis) < partitionPosition);
        do {
	        j--;
        } while (model.getObjectAxisCenter(triangles[j], axis) > partitionPosition);
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