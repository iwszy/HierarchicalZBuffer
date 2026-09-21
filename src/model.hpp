#ifndef __MODEL_HPP__
#define __MODEL_HPP__

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

	/*! @brief 导入模型
	 *  @param modelPath: 模型所在的位置
	 */
	void loadModel(std::string modelPath);
	/*! @brief 根据面的索引获取对应的顶点
	 *  @param i: 面索引
	 *	@return 面的顶点数组
	 */
	glm::vec3* getFace(int i) const;
	/*! @brief 根据面的索引获取对应的变换后的顶点
	 *  @param i: 面索引
	 *	@return 变换后的面的顶点数组
	 */
	glm::vec3* getMVPFace(int i) const;
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
	std::vector<int*> m_faces;
	/*! @brief 三角形各个轴的中心位置的数组
	 */
	glm::vec3* m_axisCenters;
	/*! @brief 三角形各个轴的最大值的数组
	 */
	glm::vec3* m_axisMaximums;
	/*! @brief 三角形各个轴的最小值的数组
	 */
	glm::vec3* m_axisMinimums;
	/*! @brief 模型名称
	 */
	std::string m_modelName;

	/*! @brief 根据面的索引以及给定的轴计算对应三角形指定轴的中心位置、最大值、最小值
	 *  @param i: 面索引
	 *  @param axis: 轴(0表示x轴，1表示y轴，2表示z轴)
	 */
	void calAxisParam(int i, int axis) const;
};

#endif // __MODEL_HPP__