#ifndef __BASIC_HIERARCHICAL_ZBUFFER_HPP__
#define __BASIC_HIERARCHICAL_ZBUFFER_HPP__

#include <glm/glm.hpp>
#include <string>
#include <chrono>
#include "zPyramid.hpp"
#include "model.hpp"
#include "scene.hpp"


/*! @brief 基础层次z-Buffer类
 *
 *  使用简单模式的层次z-Buffer进行光栅化：只用一棵 Z-max 金字塔做遮挡剔除，
 *  不事先对三角形做空间排序。在光栅化三角形时采用扫描线算法的思想。
 *  仅支持全是三角形的模型
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
	/*! @brief 展示建金字塔时间、渲染时间和总时间
	 */
	void showInfo() const;
private:
	/*! @brief Z-max 金字塔，取代了原来的指针四叉树
	 *
	 *	连续数组、没有任何指针：1024x1024 时共 1398101 个 float 约 5.6 MB，
	 *	"建金字塔"只是把整块内存填成最大深度
	 */
	ZPyramid m_pyramid;

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
	/*! @brief 构建 Z-max 金字塔所需时间
	 */
	std::chrono::duration<double, std::milli> m_buildPyramidTime;

	/*! @brief 光栅化三角形
	 *  @param face: 三角形的顶点数组
	 *  @param color: 三角形的颜色
	 */
	void rasterizeTriangle(glm::vec3* face, glm::vec3 color);
};

#endif // !__BASIC_HIERARCHICAL_ZBUFFER_HPP__
