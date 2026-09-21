#ifndef __SCENE_HPP__
#define __SCENE_HPP__

#include <glm/glm.hpp>

constexpr auto PI = 3.14159265;

/*! @brief 场景类
 *  
 *  包含场景的各种参数
 *  
 */
class Scene{
public:
	Scene();

	/*! @brief 对向量进行MVP变换
	 *  @param[in] vec: 要进行变换的向量
	 *  @return MVP变换后的向量
	 */
	glm::vec3 mvpTransform(glm::vec3& vec) const;
	/*! @brief 获取光线方向
	 *  @return 光线方向
	 */
	inline glm::vec3 getLightDirection() const { return m_lightDirection; }
	/*! @brief 获取漫反射项颜色
	 *  @return 漫反射项颜色
	 */
	inline glm::vec3 getDiffuseColor() const { return m_diffuseColor; }
	/*! @brief 获取渲染窗口宽度
	 *  @return 渲染窗口宽度
	 */
	inline int getWidth() const { return m_width; }
	/*! @brief 获取渲染窗口高度
	 *  @return 渲染窗口高度
	 */
	inline int getHeight() const{ return m_height; }
private:
	/*! @brief 相机位置
	 */
	glm::vec3 m_camera;
	/*! @brief 相机朝向方向
	 */
	glm::vec3 m_eyeDirection;
	/*! @brief 相机向上方向
	 */
	glm::vec3 m_up;
	/*! @brief 光线方向
	 */
	glm::vec3 m_lightDirection;
	/*! @brief 视平面离相机的最近距离、最远距离，视口角度
	 */
	float m_near, m_far, m_fov;
	/*! @brief MVP变换矩阵，其值等于视口变换矩阵*投影矩阵*视图变换矩阵
	 */
	glm::mat4 m_mvp;
	/*! @brief 漫反射项颜色
	 */
	glm::vec3 m_diffuseColor;
	/*! @brief 渲染窗口宽度、高度
	 */
	int m_width, m_height;

	/*! @brief 计算视图变换矩阵
	 *  @return 视图变换矩阵
	 */
	glm::mat4 getView() const;
	/*! @brief 计算投影变换矩阵
	 *  @return 投影变换矩阵
	 */
	glm::mat4 getProjection() const;
	/*! @brief 计算视口变换矩阵
	 *  @return 视口变换矩阵
	 */
	glm::mat4 getViewport() const;
};

#endif // !__SCENE_HPP__