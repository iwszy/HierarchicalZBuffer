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
/*! @brief 八叉树的最大深度，防止三角形重心全部落在同一个八分体时无限递归 */
constexpr auto MAX_OCTREE_DEPTH = 16;

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
 *	注意：包围盒建在模型空间、与相机无关，所以同一棵树可以在多帧之间复用；
 *	渲染时再把它投影到屏幕空间
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

/*! @brief 八叉树节点
 *
 *  	children: 8 个子节点，按 (x,y,z) 三个轴的"下半 / 上半"组合编号，
 *	            即 child[i] 对应 x 轴第 (i&1) 位、y 轴第 (i>>1)&1 位、z 轴第 (i>>2)&1 位；
 *	            nullptr 表示该八分体内没有三角形
 *
 *	triangles: 仅叶子节点有效，存该节点包含的三角形ID
 *
 *	leaf: 是否为叶子节点。注意不能用 children[0] == nullptr 来判断——某个
 *	      八分体为空时对应的指针就是 nullptr，而节点本身可能还有别的子节点
 *
 *	xmin..zmax: 该节点所覆盖【八分体单元】的范围（用于按空间位置排序）
 *
 *	bxmin..bzmax: 节点内三角形的紧包围盒（用于遮挡判断与金字塔下探）
 *
 *	两者都要：单元范围是不重叠的，所以按它排序得到的是真正有效的由近到远顺序；
 *	而三角形按重心分到八分体里、可能略微越界，遮挡判断必须用紧包围盒才安全。
 *
 */
struct OctreeNode {
	OctreeNode* children[8];
	std::vector<int> triangles;
	bool leaf;
	float xmin, xmax, ymin, ymax, zmin, zmax;
	float bxmin, bxmax, bymin, bymax, bzmin, bzmax;

	OctreeNode() : leaf(false) {
		for (int i = 0; i < 8; i++) {
			children[i] = nullptr;
		}
	}

	OctreeNode(const OctreeNode&) = delete;
	OctreeNode& operator=(const OctreeNode&) = delete;

	/*! @brief 是否为叶子节点 */
	bool isLeaf() const { return leaf; }

	/*! @brief 递归释放整棵子树
	 */
	~OctreeNode() {
		for (int i = 0; i < 8; i++) {
			delete children[i];
		}
	}
};

/*! @brief 层次z-Buffer类
 *
 *  使用完整模式的层次z-Buffer进行光栅化：Z-max 金字塔 + 场景加速结构。
 *  先用加速结构把三角形按空间位置组织起来，递归时对每个节点做一次遮挡判断，
 *  通过则继续往细一层下探；被遮挡则整棵子树直接跳过。
 *
 *  提供两种场景加速结构，可以切换对比：
 *    · BVH（默认）：按循环轴 + 中位数划分的二叉树
 *    · 八叉树：把模型包围盒不断八等分；八个子节点互不重叠，
 *      所以"按最小深度排序"得到的是真正有效的由近到远顺序
 *
 *  两者都建在【模型空间】，与相机无关，同一个模型只需要构建一次。
 *  仅支持全是三角形的模型
 *
 */
class HierarchicalZBuffer
{
public:
	/*! @brief 场景加速结构的类型 */
	enum class SceneStructure {
		BVH,      //!< 二叉 BVH，按循环轴 + 中位数划分
		Octree,   //!< 空间八叉树，把模型包围盒八等分
	};

	HierarchicalZBuffer(int width, int height);
	~HierarchicalZBuffer();

	HierarchicalZBuffer(const HierarchicalZBuffer&) = delete;
	HierarchicalZBuffer& operator=(const HierarchicalZBuffer&) = delete;

	/*! @brief 设置使用哪种场景加速结构
	 *  @param structure: BVH 或 Octree；切换后会在下次 rasterizeScene 时重建
	 */
	void setSceneStructure(SceneStructure structure);

	/*! @brief 将最终结果写入指定的png图片
	 */
	void render() const;
	/*! @brief 为模型构建场景加速结构
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
	/*! @brief 展示建金字塔时间、加速结构建立时间、渲染时间以及总时间
	 */
	void showInfo() const;
	/*! @brief 本帧的渲染耗时（毫秒） */
	inline double getRenderTime() const { return m_renderTime.count(); }
	/*! @brief 本帧的总耗时（含建金字塔与建树时间，毫秒） */
	inline double getTotalTime() const { return (m_buildPyramidTime + m_buildBVHTime + m_renderTime).count(); }

private:
	/*! @brief 包围盒投影到屏幕空间的结果 */
	struct ScreenBounds {
		float xmin, xmax, ymin, ymax;
		/*! @brief 节点内所有片元中【最小】的深度（也就是离相机最近的那个） */
		float zmin;
		/*! @brief false 表示包围盒与近平面相交、投影结果不可靠，本帧放弃剔除 */
		bool valid;
	};

	/*! @brief 当前使用的场景加速结构 */
	SceneStructure m_structure;
	/*! @brief BVH 根节点（m_structure == BVH 时有效） */
	BVHNode* m_bvh;
	/*! @brief 八叉树根节点（m_structure == Octree 时有效） */
	OctreeNode* m_octree;
	/*! @brief 当前加速结构是为哪个模型构建的，用于判断是否需要重建 */
	int m_bvhFaceNum;
	std::string m_bvhModelName;
	SceneStructure m_bvhStructure;
	/*! @brief 构建时复用的三角形索引数组与八叉树划分时的临时缓冲 */
	std::vector<int> m_triangles, m_octantIdx, m_octantTmp;
	/*! @brief Z-max 金字塔，取代了原来的指针四叉树 */
	ZPyramid m_pyramid;
	/*! @brief 本帧的 MVP 矩阵，用于把模型空间包围盒投影到屏幕空间 */
	glm::mat4 m_mvp;
	/*! @brief 本帧的环境光强度，与单向光一起决定面的亮度 */
	float m_ambient = 0.0f;
	/*! @brief 本帧的近平面距离（正值），用于近平面裁剪 */
	float m_nearDistance = 0.3f;
	/*! @brief 本帧的渲染窗口尺寸，用于视锥裁剪 */
	float m_screenWidth = 1024.0f, m_screenHeight = 1024.0f;

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
	/*! @brief 构建场景加速结构所需时间（被复用时为 0） */
	std::chrono::duration<double, std::milli> m_buildBVHTime;

	/*! @brief 把模型空间的包围盒投影到屏幕空间
	 *
	 *  投影 8 个角点后取包围盒：透视投影把凸多面体映射成凸多边形，
	 *  它的屏幕包围盒与深度极值必然在角点上取到。
	 *
	 *  @param xmin/xmax/ymin/ymax/zmin/zmax: 模型空间包围盒
	 *  @param mvp: 本帧的 MVP 变换矩阵
	 *  @return 屏幕空间的包围盒与最小深度
	 */
	static ScreenBounds projectBounds(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, const glm::mat4& mvp);
	/*! @brief 遮挡判断 + 向下找到能包含该包围盒的最细金字塔单元
	 *  @param[in] sb: 节点的屏幕投影包围盒
	 *  @param[in,out] level/qx/qy: 金字塔层次与单元坐标，函数内会尽量向下推进
	 *  @return false 表示整个节点被已有几何遮挡，可以整棵跳过
	 */
	bool descendPyramid(const ScreenBounds& sb, int& level, int& qx, int& qy) const;
	/*! @brief 逐个三角形做遮挡判断并光栅化
	 *  @param triangles: 三角形ID列表
	 *  @param model: 模型类
	 *  @param lightDirection: 场景光线方向
	 *  @param diffuseColor: 场景的漫反射颜色
	 */
	void rasterizeTriangles(const std::vector<int>& triangles, Model& model, const glm::vec3& lightDirection, const glm::vec3& diffuseColor);

	/*! @brief 递归光栅化场景（BVH 版本）
	 *  @param bvhNode: 当前BVH节点
	 *  @param sb: 该节点包围盒在本帧屏幕空间下的投影结果
	 *  @param level/qx/qy: 当前使用的金字塔层次与单元坐标
	 */
	void recursiveRasterizeBVH(BVHNode* bvhNode, const ScreenBounds& sb, int level, int qx, int qy, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor);
	/*! @brief 递归光栅化场景（八叉树版本）
	 *
	 *  与 BVH 版本的区别只在"子节点怎么排"：八叉树的 8 个子节点是互不重叠的
	 *  空间单元，所以按最小深度排序能得到真正有效的由近到远顺序。
	 *
	 *  @param octNode: 当前八叉树节点
	 *  @param sb: 该节点包围盒在本帧屏幕空间下的投影结果
	 *  @param level/qx/qy: 当前使用的金字塔层次与单元坐标
	 */
	void recursiveRasterizeOctree(OctreeNode* octNode, const ScreenBounds& sb, int level, int qx, int qy, Model& model, glm::vec3 lightDirection, glm::vec3 diffuseColor);

	/*! @brief 构建BVH
	 *  @param triangles: 三角形ID数组
	 *  @param left: 该节点对应的三角形ID集合在ID数组的起始索引
	 *  @param right: 该节点对应的三角形ID集合在ID数组的终止索引
	 *  @param axis: 该节点划分对应的轴(0表示x轴，1表示y轴，2表示z轴)
	 *  @param model: 模型类
	 *	@return 当前构建的BVH节点
	 */
	BVHNode* buildBVH(int* triangles, int left, int right, int axis, Model& model);
	/*! @brief 构建八叉树
	 *  @param triangles: 当前节点负责的三角形ID数组
	 *  @param count: 三角形个数
	 *  @param xmin/xmax/ymin/ymax/zmin/zmax: 当前八分体单元的范围
	 *  @param depth: 当前深度，用于限制最大递归层数
	 *  @param model: 模型类
	 *	@return 当前构建的八叉树节点
	 */
	OctreeNode* buildOctree(int* triangles, int count, float xmin, float xmax, float ymin, float ymax, float zmin, float zmax, int depth, Model& model);
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