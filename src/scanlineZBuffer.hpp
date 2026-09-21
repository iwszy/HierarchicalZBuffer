#ifndef __SCANLINE_ZBUFFER_H__
#define __SCANLINE_ZBUFFER_H__

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include "model.hpp"
#include "scene.hpp"

#define EPSILON 1e-5

/*! @brief 三角形表
 *
 *  dzx: 三角形上的点x值每加1所对应的z值的增量
 *
 *	dzy: 三角形上的点y值每减1所对应的z值的增量
 *
 *	color: 三角形的颜色
 *
 */
struct TriangleTable {
	float dzx, dzy;
	glm::vec3 color;
};

/*! @brief 分类边表
 *
 *  x: 边的上顶点的x坐标
 *
 *	dx: 边上的点y值每减1所对应的x值的增量
 *
 *	dy: 边所跨越的扫描线数
 *
 *	z: 边的上顶点的z值
 *
 *	id: 边所在的三角形的id
 *
 */
struct ClassifyEdgeTable {
	float x;
	float dx;
	int dy;
	float z;
	int id;
};

/*! @brief 活化边表
 *
 *  xLeft: 左侧边的上顶点的x坐标
 *
 *  xRight: 右侧边的上顶点的x坐标
 *
 *	dxLeft: 左侧边上的点y值每减1所对应的x值的增量
 *
 *	dxRight: 右侧边上的点y值每减1所对应的x值的增量
 *
 *	dyLeft: 左侧边所跨越的扫描线数
 *
 *	dyRight: 右侧边所跨越的扫描线数
 *
 *	z: 边的上顶点的z值
 *
 *	dzx: 该边对应的三角形上的点x值每加1所对应的z值的增量
 *
 *	dzy: 该边所在的三角形上的点y值每减1所对应的z值的增量
 *
 *	id: 边所在的三角形的id
 *
 */
struct ActiveEdgeTable {
	float xLeft, xRight;
	float dxLeft, dxRight;
	int dyLeft, dyRight;
	float z, dzx, dzy;
	int id;
};

/*! @brief 扫描线z-Buffer类
 *
 *  使用扫描线z-Buffer进行光栅化，仅支持全是三角形的模型
 *
 */
class ScanlineZBuffer{
public:
	ScanlineZBuffer(int width, int height);
	~ScanlineZBuffer();

	/*! @brief 将最终结果写入指定的png图片
	 */
	void render() const;
	/*! @brief 将场景光栅化
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void rasterizeScene(Model &model, Scene &scene);
	/*! @brief 展示\建表时间、渲染时间以及总时间
	 */
	void showInfo() const;
	/*! @brief 设置渲染模式
	 *  @param modelNum: 渲染模式，1表示特化模式，其余为经典模式
	 */
	inline void setMode(int modelNum) { mode = modelNum; }
private:
	/*! @brief 三角形字典，可以通过三角形id，快速查到对应的三角形
	 */
	std::unordered_map<int, TriangleTable> m_triangles;
	/*! @brief 分类多边形表
	 */
	std::vector<ClassifyEdgeTable>* m_classifyEdgeTables;
	/*! @brief 活化三角形ID集合
	 */
	std::unordered_set<int> m_activeTriangles;
	/*! @brief 活化边表，可以通过多边形ID快速查询到对应活化的边
	 */
	std::unordered_map<int, ActiveEdgeTable> m_activeEdgeTables;
	/*! @brief 渲染模式
	 *
	 *	1表示特化模式，其余为经典模式
	 *
	 */
	int mode;

	/*! @brief 渲染窗口宽度、高度
	 */
	int m_width, m_height;
	/*! @brief z-buffer
	 */
	float* m_zBuffer;
	/*! @brief 最终图像的RGBA数据，每4位代表一个像素的RGBA
	 */
	unsigned char* m_image;
	/*! @brief 渲染的模型的名称
	 */
	std::string m_modelName;
	/*! @brief 建表所需时间
	 */
	std::chrono::duration<double, std::milli> m_buildTableTime;
	/*! @brief 渲染所需时间
     */
	std::chrono::duration<double, std::milli> m_renderTime;

	/*! @brief 生成分类的边表
	 *  @param model: 模型类
	 *  @param scene: 场景类
	 */
	void generateTables(Model &model, Scene &scene);
	/*! @brief 光栅化三角形
	 *  @param face: 三角形的顶点数组
	 *  @param color: 三角形的颜色
	 */
	void rasterizeTriangle(glm::vec3* face, glm::vec3 color);
};

#endif // !__SCANLINE_ZBUFFER_H__