#ifndef __QUADNODE_HPP__
#define __QUADNODE_HPP__

#include <limits>

constexpr auto maxFloat = std::numeric_limits <float>::max();
constexpr auto minFloat = std::numeric_limits <float>::min();

/*! @brief 四叉树节点
 *
 *  parent: 父节点
 *
 *	children: 4个子节点
 *
 *	level: 节点的级别
 *
 *	depth: 该节点对应的像素区域的最大深度值
 *
 *	left: 该节点对应的像素区域的左边界
 *
 *	right: 该节点对应的像素区域的右边界 + 1
 *
 *	bottom: 该节点对应的像素区域的下边界
 *
 *	top: 该节点对应的像素区域的上边界 + 1
 *
 */
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
		children = new QuadNode * [4];
	}

	/*! @brief 递归删除节点
	 */
	void free() const{
		for (int i = 0; i < 4; i++) {
			if (children[i] != nullptr) {
				children[i]->free();
				children[i] = nullptr;
			}
		}
		delete this;
	}
};

#endif
