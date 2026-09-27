#ifndef __BASIC_ZBUFFER_HPP__
#define __BASIC_ZBUFFER_HPP__

#include <glm/glm.hpp>
#include <string>
#include <chrono>
#include "model.hpp"
#include "scene.hpp"

#define EPSILON 1e-5
/*! @brief 基础z-Buffer类
 *
 *  使用最基础的z-Buffer进行光栅化，即对每个三角形的包围盒内的像素逐个判断，
 *  仅支持全是三角形的模型
 *
 */
class BasicZBuffer {
public:
	BasicZBuffer(int width, int height);
	~BasicZBuffer();

	/*! @brief 将最终结果写入指定的png图片
	 */
	void render() const;
	/*! @brief 将场景光栅化
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void rasterizeScene(Model& model, Scene& scene);
	/*! @brief 展示渲染时间
	 */
	void showInfo() const;
	/*! @brief 本帧的渲染耗时（毫秒） */
	inline double getRenderTime() const { return m_renderTime.count(); }
	/*! @brief 本帧的总耗时（毫秒） */
	inline double getTotalTime() const { return m_renderTime.count(); }
private:
	/*! @brief 渲染窗口的宽度与高度
	 */
	int m_width, m_height;
	/*! @brief z-buffer
	 */
	float* m_zBuffer;
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

	/*! @brief 将三角形光栅化
	 *  @param vertices: 进行了MVP变换后的三角形的3个顶点的坐标
	 *  @param color: 该三角形的颜色
	 */
	void rasterizeTriangle(glm::vec3* vertices, glm::vec3 color);
	/*! @brief 计算一个点在三角形中的重心坐标
	 *  @param vertices: 进行了MVP变换后的三角形的3个顶点的坐标
	 *  @param point: 需要计算重心坐标的点的坐标
	 *  @return 该点的重心坐标
	 */
	glm::vec3 baryCentric(glm::vec3* vertices, glm::vec2 point);
};

#endif // !__BASIC_ZBUFFER_HPP__