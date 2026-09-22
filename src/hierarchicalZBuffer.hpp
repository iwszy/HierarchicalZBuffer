#ifndef __HIERARCHICAL_ZBUFFER_HPP__
#define __HIERARCHICAL_ZBUFFER_HPP__

#include <string>
#include <vector>
#include <chrono>
#include "quadNode.hpp"
#include "model.hpp"
#include "scene.hpp"

constexpr auto MAX_TRIANGLE = 20;

#define EPSILON 1e-5

/*! @brief BVH节点
 *
 *  left: 左子节点
 *
 *	right: 右子节点
 *
 *	triangles: 若为非叶子节点，则为空；否则，其表示该叶子结点包含的三角形的ID
 *
 *	xmin, xmax: 该节点包围盒x轴方向上的最小值、最大值
 *
 *	ymin, ymax: 该节点包围盒y轴方向上的最小值、最大值
 *
 *	zmin, zmax: 该节点包围盒z轴方向上的最小值、最大值
 *
 */
struct BVHNode {
	BVHNode* left, *right;
	std::vector<int> triangles;
	float xmin, xmax, ymin, ymax, zmin, zmax;

	BVHNode() : left(nullptr), right(nullptr) {}

	BVHNode(const BVHNode&) = delete;
	BVHNode& operator=(const BVHNode&) = delete;

	/*! @brief 递归释放整棵子树（子节点的析构会继续向下递归）
	 */
	~BVHNode() {
		delete left;
		delete right;
	}
};

/*! @brief 层次z-Buffer类
 *
 *  使用完整模式的层次z-Buffer进行光栅化，即使用四叉树的同时用BVH预排序，
 *	仅支持全是三角形的模型
 *
 */
class HierarchicalZBuffer
{
public:
	HierarchicalZBuffer(int width, int height);
	~HierarchicalZBuffer();

	HierarchicalZBuffer(const HierarchicalZBuffer&) = delete;
	HierarchicalZBuffer& operator=(const HierarchicalZBuffer&) = delete;

	/*! @brief 将最终结果写入指定的png图片
	 */
	void render() const;
	/*! @brief 将场景光栅化
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void rasterizeScene(Model& model, Scene& scene);
	/*! @brief 展示建树时间、BVH建立时间、渲染时间以及总时间
	 */
	void showInfo() const;

private:
	/*! @brief BVH根节点
	 */
	BVHNode* m_bvh;
	/*! @brief 四叉树根节点
	 */
	QuadNode* m_quadTree;
	/*! @brief 每个像素所对应的四叉树节点的数组
	 */
	QuadNode** m_pixelQuadNodes;

	/*! @brief 渲染窗口宽度、高度
	 */
	int m_width, m_height;
	/*! @brief 最终图像的RGBA数据，每4位代表一个像素的RGBA
	 */
	unsigned char* m_image;
	/*! @brief 缓冲区中是否已有渲染结果
	 *
	 *	用于判断同一个对象再次光栅化前是否需要清空缓冲，
	 *	保证"只渲染其中一种算法"和"依次渲染所有算法"结果一致
	 */
	bool m_dirty;
	/*! @brief 渲染的模型的名称
	 */
	std::string m_modelName;
	/*! @brief 渲染所需时间
	 */
	std::chrono::duration<double, std::milli> m_renderTime;
	/*! @brief 构建四叉树所需时间
	 */
	std::chrono::duration<double, std::milli> m_buildTreeTime;
	/*! @brief 构建BVH所需时间
	 */
	std::chrono::duration<double, std::milli> m_buildBVHTime;

	/*! @brief 构建初始的四叉树
	 */
	void buildQuadTree();
	/*! @brief 构建BVH
	 *  @param triangles: 三角形ID数组
	 *  @param left: 该节点对应的三角形ID集合在ID数组的起始索引
	 *  @param right: 该节点对应的三角形ID集合在ID数组的终止索引
	 *  @param axis: 该节点划分对应的轴(0表示x轴，1表示y轴，2表示z轴)
	 *  @param model: 模型类
	 *	@return 当前构建的BVH节点
	 */
	BVHNode* buildBVH(int* triangles, int left, int right, int axis, Model& model);
	/*! @brief 递归光栅化场景
	 *  @param bvhNode: 当前BVH节点
	 *  @param quadNode: 当前四叉树节点
	 *  @param model: 模型类
	 *  @param lightDirection: 场景光线方向
	 *  @param diffuseColor: 场景的漫反射颜色
	 */
	void recursiveRasterizeScene(BVHNode* bvhNode, QuadNode* quadNode, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor);
	/*! @brief 光栅化三角形
	 *  @param face: 三角形的顶点数组
	 *  @param color: 三角形的颜色
	 */
	void rasterizeTriangle(glm::vec3* face, glm::vec3 color);
	/*! @brief 更新四叉树
	 *
	 *  根据给定的四叉树节点向上更新深度值
	 *
	 *  @param node: 要更新的四叉树节点
	 */
	void update(QuadNode* node);
	/*! @brief 判断三角形是否需要光栅化
	 *  @param[in] vertices: 三角形的顶点数组
	 *	@return 是否需要光栅化三角形
	 */
	bool isNeedRasterize(glm::vec3* vertices) const;
	/*! @brief 划分数组
	 *
	 *  根据给定位置k将数组划分为以下形式: 数组前k个的值小于等于数组第k个的值，
	 *  数组后k个的值大于等于数组第k个的值
	 *
	 *  @param triangles: 三角形ID数组
	 *  @param left: 起始索引
	 *  @param right: 终止索引
	 *  @param axis: 根据哪个轴向划分
	 *  @param k: 给定的位置k
	 *  @param model: 模型类
	 */
	void partition(int* triangles, int left, int right, int axis, int k, Model& model);
	/*! @brief 判断给定的四叉树节点是否包含BVH节点
	 *  @param bvhNode: BVH节点
	 *  @param quadNode: 四叉树节点
	 *	@return 四叉树节点是否包含BVH节点
	 */
	bool isInQuadNode(BVHNode* bvhNode, QuadNode* quadNode);
};

#endif // __HIERARCHICAL_ZBUFFER_HPP__