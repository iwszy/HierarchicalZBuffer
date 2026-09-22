#ifndef __MODEL_HPP__
#define __MODEL_HPP__

#include <array>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "scene.hpp"

/*! @brief 模型类
 *
 *  包含模型的导入操作以及存储模型的数据
 *	仅支持全是三角形的obj模型
 *
 */
class Model {
public:
	Model();
	~Model();

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	/*! @brief 导入模型
	 *  @param modelPath: 模型所在的位置
	 */
	void loadModel(std::string modelPath);
	/*! @brief 根据面的索引获取对应的顶点
	 *  @param i: 面索引
	 *  @param[out] face: 面的 3 个顶点，由调用方提供缓冲区
	 */
	void getFace(int i, glm::vec3 face[3]) const;
	/*! @brief 根据面的索引获取对应的变换后的顶点
	 *  @param i: 面索引
	 *  @param[out] face: 变换后的面的 3 个顶点，由调用方提供缓冲区
	 */
	void getMVPFace(int i, glm::vec3 face[3]) const;
	/*! @brief 将所有顶点进行MVP变换
	 *  @param scene: 场景类
	 */
	void mvpTransform(Scene& scene);
	/*! @brief 根据面的索引以及给定的轴获取对应三角形指定轴的中心位置
	 *  @param i: 面索引
	 *  @param axis: 轴(0表示x轴，1表示y轴，2表示z轴)
	 *	@return 指定三角形指定轴的中心位置
	 */
	float getAxisCenter(int i, int axis) const { return m_axisCenters[i][axis]; }
	/*! @brief 根据面的索引以及给定的轴获取对应三角形指定轴的最大值
	 *  @param i: 面索引
	 *  @param axis: 轴(0表示x轴，1表示y轴，2表示z轴)
	 *	@return 指定三角形指定轴的最大值
	 */
	float getAxisMaximum(int i, int axis) const { return m_axisMaximums[i][axis]; }
	/*! @brief 根据面的索引以及给定的轴获取对应三角形指定轴的最小值
	 *  @param i: 面索引
	 *  @param axis: 轴(0表示x轴，1表示y轴，2表示z轴)
	 *	@return 指定三角形指定轴的最小值
	 */
	float getAxisMinimum(int i, int axis) const { return m_axisMinimums[i][axis]; }
	/*! @brief 计算所有面的所有轴的中心位置、最大值、最小值
	 */
	void calAxisParams();
	/*! @brief 根据面的索引以及给定的轴获取对应三角形【模型空间】指定轴的中心位置
	 *  @param i: 面索引
	 *  @param axis: 轴(0表示x轴，1表示y轴，2表示z轴)
	 *	@return 模型空间下指定轴的中心位置
	 */
	float getObjectAxisCenter(int i, int axis) const { return m_objectAxisCenters[i][axis]; }
	/*! @brief 根据面的索引以及给定的轴获取对应三角形【模型空间】指定轴的最大值
	 */
	float getObjectAxisMaximum(int i, int axis) const { return m_objectAxisMaximums[i][axis]; }
	/*! @brief 根据面的索引以及给定的轴获取对应三角形【模型空间】指定轴的最小值
	 */
	float getObjectAxisMinimum(int i, int axis) const { return m_objectAxisMinimums[i][axis]; }
	/*! @brief 计算模型空间下所有面的中心、最大值、最小值
	 *
	 *	与相机无关，供"建在模型空间、可以跨帧复用"的 BVH 使用
	 */
	void calAxisParamsObject();
	/*! @brief 清除变换后的顶点数组
	 */
	void clear() { delete[] m_mvpVertices; m_mvpVertices = nullptr; }

	/*! @brief 获取模型的顶点数量
	 *  @return 模型的顶点数量
	 */
	inline int getVertexNum() const { return m_vertexNum; }
	/*! @brief 获取模型的面数量
	 *  @return 模型的面数量
	 */
	inline int getFaceNum() const { return m_faceNum; }
	/*! @brief 获取模型名称
	 *  @return 模型名称
	 */
	inline std::string getModelName() const { return m_modelName; }
private:
	/*! @brief 模型的顶点数量、面数量
	 */
	int m_vertexNum, m_faceNum;
	/*! @brief 模型的顶点数组
	 */
	std::vector<glm::vec3> m_vertices;
	/*! @brief 模型进行MVP变换后的顶点数组
	 */
	glm::vec3* m_mvpVertices;
	/*! @brief 模型的面数组，其中面使用3个顶点的索引进行存储
	 */
	std::vector<std::array<int, 3>> m_faces;
	/*! @brief 三角形各个轴的中心位置的数组
	 */
	glm::vec3* m_axisCenters;
	/*! @brief 三角形各个轴的最大值的数组
	 */
	glm::vec3* m_axisMaximums;
	/*! @brief 三角形各个轴的最小值的数组
	 */
	glm::vec3* m_axisMinimums;
	/*! @brief 模型空间下三角形各个轴的中心、最大值、最小值的数组
	 */
	glm::vec3* m_objectAxisCenters;
	glm::vec3* m_objectAxisMaximums;
	glm::vec3* m_objectAxisMinimums;
	/*! @brief 模型名称
	 */
	std::string m_modelName;

	/*! @brief 计算所有面的轴向包围盒的公共实现
	 *  @param verts: 顶点数组（模型空间或屏幕空间）
	 *  @param centers: 接收中心位置
	 *  @param maximums: 接收最大值
	 *  @param minimums: 接收最小值
	 */
	void calAxisParamsImpl(const glm::vec3* verts, glm::vec3* centers, glm::vec3* maximums, glm::vec3* minimums) const;
};

#endif // __MODEL_HPP__