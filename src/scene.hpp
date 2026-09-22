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
	/*! @brief 场景参数
	 *
	 *  默认值与重构前完全一致，所以用默认参数构造的 Scene 渲染结果逐位不变；
	 *  建筑类等需要把相机放进模型内部的场景，用 Config 指定机位、视锥与光照。
	 */
	struct Config {
		glm::vec3 camera{ 0.0f, 0.0f, 1.8f };
		glm::vec3 eyeDirection{ 0.0f, 0.0f, -1.0f };
		glm::vec3 up{ 0.0f, 1.0f, 0.0f };
		glm::vec3 lightDirection{ 0.0f, 0.0f, 1.0f };
		glm::vec3 diffuseColor{ 0.5f, 0.5f, 0.5f };
		/*! @brief 视平面离相机的最近、最远距离，以及视口张角 */
		float nearPlane{ -0.3f }, farPlane{ -100.0f }, fov{ 90.0f };
		/*! @brief 环境光强度
		 *
		 *  0 表示只有单向光，与重构前一致。单向光对闭合的凸模型够用，
		 *  对建筑内部不行：背光的那一半面会渲染成纯黑。
		 */
		float ambient{ 0.0f };
		int width{ 1024 }, height{ 1024 };
	};

	Scene();
	/*! @brief 用指定参数构造场景
	 *  @param[in] config: 场景参数
	 */
	explicit Scene(const Config& config);

	/*! @brief 对向量进行MVP变换
	 *  @param[in] vec: 要进行变换的向量
	 *  @return MVP变换后的向量
	 */
	glm::vec3 mvpTransform(const glm::vec3& vec) const;
	/*! @brief 获取 MVP 变换矩阵（用于把模型空间包围盒投影到屏幕空间）
	 *  @return MVP 变换矩阵
	 */
	inline const glm::mat4& getMVP() const { return m_mvp; }
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
	/*! @brief 获取环境光强度
	 *  @return 环境光强度
	 */
	inline float getAmbient() const { return m_ambient; }
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
	/*! @brief 环境光强度
	 */
	float m_ambient;
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
