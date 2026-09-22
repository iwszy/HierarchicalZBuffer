#ifndef __QUADNODE_HPP__
#define __QUADNODE_HPP__

#include <limits>
#include <stack>
#include <glm/glm.hpp>

constexpr auto maxFloat = std::numeric_limits <float>::max();
constexpr auto minFloat = std::numeric_limits <float>::lowest();

/*! @brief 与各算法头文件里 EPSILON 宏取值一致的常量
 *
 *	这里不用 EPSILON 宏：本文件会被那些头文件包含，处理顺序上宏还没被定义
 */
constexpr double quadtreeEpsilon = 1e-5;

struct QuadNode {
	QuadNode* parent;
	QuadNode** children;
	int level;
	float depth;
	int left, right, top, bottom;

	QuadNode(QuadNode* parent, int level, int l, int r, int b, int t) {
		this->parent = parent;
		this->level = level;
		left = l;
		right = r;
		top = t;
		bottom = b;
		depth = maxFloat;
		children = new QuadNode * [4]();
	}

	QuadNode(const QuadNode&) = delete;
	QuadNode& operator=(const QuadNode&) = delete;

	/*! @brief 递归释放整棵子树
	 *
	 *  先删除 4 个子节点（子节点的析构会继续向下递归），再释放子节点指针数组。
	 *  原来这里只 delete this，children 数组本身从未释放，是主要泄漏点。
	 */
	~QuadNode() {
		for (int i = 0; i < 4; i++) {
			delete children[i];
		}
		delete[] children;
	}
};

/*! @brief 按渲染窗口分辨率构建一棵完整的四叉树
 *
 *  根节点覆盖整个渲染窗口，四个子节点把父节点区域四等分，一直划分到 1x1 像素为止；
 *  每个 1x1 的叶子节点同时登记到 pixelQuadNodes 中，便于按像素坐标 O(1) 定位。
 *
 *  @param[in] width: 渲染窗口宽度
 *  @param[in] height: 渲染窗口高度
 *  @param[in,out] pixelQuadNodes: 长度不少于 width*height 的数组，用于接收像素节点
 *  @param[in] oldRoot: 上一次构建的树，若非空则先释放，使本函数可以重复调用
 *  @return 新构建的四叉树根节点（由调用方负责 delete）
 */
inline QuadNode* buildQuadTree(int width, int height, QuadNode** pixelQuadNodes, QuadNode* oldRoot) {
	delete oldRoot;
	std::stack<QuadNode*> stack;
	QuadNode* root = new QuadNode(nullptr, 0, 0, width, 0, height);
	stack.push(root);
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
					pixelQuadNodes[child->bottom * width + child->left] = node->children[i];
					child->children[0] = nullptr;
					child->children[1] = nullptr;
					child->children[2] = nullptr;
					child->children[3] = nullptr;
				} else {
					stack.push(node->children[i]);
				}
			}
		}
	}
	return root;
}

/*! @brief 从给定节点开始逐级向上更新节点对应区域的最大深度值
 *
 *  @param[in] node: 起始节点，一般为刚被写入深度的像素节点的父节点
 */
inline void updateQuadTreeDepth(QuadNode* node) {
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
		if (tempNode->depth - maxDepth > quadtreeEpsilon) {
			//如果子节点的最大深度值小于当前节点的最大深度值，则更新此节点以及父节点
			tempNode->depth = maxDepth;
			tempNode = tempNode->parent;
		} else {
			break;
		}
	}
}

/*! @brief 判断三角形是否需要光栅化（是否有可能被已有几何遮挡）
 *
 *  找出能够覆盖该三角形的四叉树节点（即三个顶点对应像素节点的公共祖先），
 *  再比较三角形的最小深度与该节点的最大深度。
 *
 *  @param[in] pixelQuadNodes: 像素节点数组
 *  @param[in] width: 渲染窗口宽度
 *  @param[in] height: 渲染窗口高度
 *  @param[in] vertices: 三角形的 3 个顶点
 *  @return 是否需要光栅化该三角形
 */
inline bool isNeedRasterize(QuadNode* const* pixelQuadNodes, int width, int height, const glm::vec3* vertices) {
	//顶点可能落在窗口外，先夹取到窗口内，避免越界访问 pixelQuadNodes
	int vx[3], vy[3];
	for (int i = 0; i < 3; i++) {
		vx[i] = glm::clamp(static_cast<int>(vertices[i].x), 0, width - 1);
		vy[i] = glm::clamp(static_cast<int>(vertices[i].y), 0, height - 1);
	}
	QuadNode* node0 = pixelQuadNodes[vy[0] * width + vx[0]];
	QuadNode* node1 = pixelQuadNodes[vy[1] * width + vx[1]];
	QuadNode* node2 = pixelQuadNodes[vy[2] * width + vx[2]];
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

#endif
