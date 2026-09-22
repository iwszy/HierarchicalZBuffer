#ifndef __HIERARCHICAL_ZBUFFER_HPP__
#define __HIERARCHICAL_ZBUFFER_HPP__

#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>
#include "zPyramid.hpp"
#include "model.hpp"
#include "scene.hpp"

constexpr auto MAX_TRIANGLE = 20;

/*! @brief BVH节点
 *
 *  left: 左子节点
 *
 *	right: 右子节点
 *
 *	triangles: 若为非叶子节点，则为空；否则，其表示该叶子结点包含的三角形的ID
 *
 *	xmin, xmax, ymin, ymax, zmin, zmax: 该节点在【模型空间】下的包围盒
 *
 *	注意：包围盒建在模型空间、与相机无关，所以同一棵 BVH 可以在多帧之间复用；
 *	渲染时再把它投影到屏幕空间，见 projectBounds()
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
 *  使用完整模式的层次z-Buffer进行光栅化：Z-max 金字塔 + BVH。
 *  先用 BVH 把三角形按空间位置组织起来，递归时对每个 BVH 节点做一次遮挡判断，
 *  通过则继续往细一层下探；被遮挡则整棵子树直接跳过。
 *
 *  BVH 建在【模型空间】，与相机无关，同一个模型只需要构建一次，可以在多帧之间
 *  反复复用；每帧只需要重建 Z-max 金字塔、重算屏幕投影即可。
 *  仅支持全是三角形的模型
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
	/*! @brief 为模型构建 BVH
	 *
	 *  建在模型空间、与相机无关，同一个模型只需要构建一次。
	 *  rasterizeScene 会在发现模型变化时自动调用它，一般不需要手动调用。
	 *
	 *  @param model: 模型类
	 */
	void buildScene(Model& model);
	/*! @brief 将场景光栅化
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void rasterizeScene(Model& model, Scene& scene);
	/*! @brief 展示建金字塔时间、BVH建立时间、渲染时间以及总时间
	 */
	void showInfo() const;

private:
	/*! @brief BVH 节点的包围盒投影到屏幕空间的结果 */
	struct ScreenBounds {
		float xmin, xmax, ymin, ymax;
		/*! @brief 节点内所有片元中【最小】的深度（也就是离相机最近的那个） */
		float zmin;
		/*! @brief false 表示包围盒与近平面相交、投影结果不可靠，本帧放弃剔除 */
		bool valid;
	};

	/*! @brief BVH根节点 */
	BVHNode* m_bvh;
	/*! @brief 当前 m_bvh 是为哪个模型构建的，用于判断是否需要重建 */
	int m_bvhFaceNum;
	std::string m_bvhModelName;
	/*! @brief 构建 BVH 时复用的三角形索引数组 */
	std::vector<int> m_triangles;
	/*! @brief Z-max 金字塔，取代了原来的指针四叉树 */
	ZPyramid m_pyramid;
	/*! @brief 本帧的 MVP 矩阵，用于把模型空间包围盒投影到屏幕空间 */
	glm::mat4 m_mvp;

	/*! @brief 渲染窗口宽度、高度 */
	int m_width, m_height;
	/*! @brief 最终图像的RGBA数据，每4位代表一个像素的RGBA */
	unsigned char* m_image;
	/*! @brief 缓冲区中是否已有渲染结果 */
	bool m_dirty;
	/*! @brief 渲染的模型的名称 */
	std::string m_modelName;
	/*! @brief 渲染所需时间 */
	std::chrono::duration<double, std::milli> m_renderTime;
	/*! @brief 构建 Z-max 金字塔所需时间 */
	std::chrono::duration<double, std::milli> m_buildPyramidTime;
	/*! @brief 构建BVH所需时间（BVH 被复用时为 0） */
	std::chrono::duration<double, std::milli> m_buildBVHTime;

	/*! @brief 把 BVH 节点的模型空间包围盒投影到屏幕空间
	 *
	 *  投影 8 个角点后取包围盒：透视投影把凸多面体映射成凸多边形，
	 *  它的屏幕包围盒与深度极值必然在角点上取到。
	 *
	 *  @param node: BVH 节点
	 *  @param mvp: 本帧的 MVP 变换矩阵
	 *  @return 屏幕空间的包围盒与最小深度
	 */
	static ScreenBounds projectBounds(const BVHNode* node, const glm::mat4& mvp);
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
	 *  @param sb: 该节点包围盒在本帧屏幕空间下的投影结果
	 *  @param level: 当前使用的金字塔层次（0 为逐像素层，rootLevel() 为最粗层）
	 *  @param qx: 该层次下单元的横坐标
	 *  @param qy: 该层次下单元的纵坐标
	 *  @param model: 模型类
	 *  @param lightDirection: 场景光线方向
	 *  @param diffuseColor: 场景的漫反射颜色
	 */
	void recursiveRasterizeScene(BVHNode* bvhNode, const ScreenBounds& sb, int level, int qx, int qy, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor);
	/*! @brief 光栅化三角形
	 *  @param face: 三角形的顶点数组
	 *  @param color: 三角形的颜色
	 */
	void rasterizeTriangle(glm::vec3* face, glm::vec3 color);
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
};

#endif // __HIERARCHICAL_ZBUFFER_HPP__
