#ifndef __BASIC_HIERARCHICAL_ZBUFFER_HPP__
#define __BASIC_HIERARCHICAL_ZBUFFER_HPP__

#include <glm/glm.hpp>
#include <string>
#include <chrono>
#include "quadNode.hpp"
#include "model.hpp"
#include "scene.hpp"

#define EPSILON 1e-5

/*! @brief 基础层次z-Buffer类
 *
 *  使用简单模式的层次z-Buffer进行光栅化，即仅使用四叉树而不进行预先排序。
 *	在光栅化三角形时采用扫描线算法的思想。
 *	仅支持全是三角形的模型
 *
 */
class BasicHierarchicalZBuffer
{
public:
	BasicHierarchicalZBuffer(int width, int height);
	~BasicHierarchicalZBuffer();

	BasicHierarchicalZBuffer(const BasicHierarchicalZBuffer&) = delete;
	BasicHierarchicalZBuffer& operator=(const BasicHierarchicalZBuffer&) = delete;

	/*! @brief 将最终结果写入指定的png图片
	 */
	void render() const;
	/*! @brief 将场景光栅化
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void rasterizeScene(Model &model, Scene &scene);
	/*! @brief 展示建树时间、渲染时间和总时间
	 */
	void showInfo() const;
private:
	/*! @brief 四叉树的根节点
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
	/*! @brief 渲染的模型的名称
	 */
	std::string m_modelName;
	/*! @brief 渲染所需时间
	 */
	std::chrono::duration<double, std::milli> m_renderTime;
	/*! @brief 构建四叉树所需时间
	 */
	std::chrono::duration<double, std::milli> m_buildTreeTime;

	/*! @brief 构建初始的四叉树
	 */
	void buildQuadTree();
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
	 *  @param vertices: 三角形的顶点数组
	 *	@return 是否需要光栅化三角形
	 */
	bool isNeedRasterize(glm::vec3* vertices) const;
};

#endif // !__BASIC_HIERARCHICAL_ZBUFFER_HPP__