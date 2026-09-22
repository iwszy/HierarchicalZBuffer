#ifndef __SCANLINE_ZBUFFER_H__
#define __SCANLINE_ZBUFFER_H__

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <chrono>
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
	/*! @brief 左侧边进入扫描线时的参考点：该边此刻的 x、所在行，以及深度平面在该点的取值
	 *
	 *	深度平面是 z(x,y) = zRef + (yRef - y) * dzy + (x - xRef) * dzx，渲染时直接代入求值。
	 *	原来是把 z 沿扫描线逐行累加（z += dzx * dxLeft + dzy），这两个增量常常是
	 *	一对量级相同、符号相反的大数，相减本身就带误差，再乘上成百上千行就积成了
	 *	可见的深度错误（实测最大越界幅度到过 36.6，而整个视锥才 [-1, 1]）。
	 */
	float xRef, zRef;
	int yRef;
	float dzx, dzy;
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
	/*! @brief 三角形表，按面 id 直接索引
	 *
	 *	原来是 unordered_map<int,TriangleTable>，每次取颜色都要哈希一次
	 */
	std::vector<TriangleTable> m_triangles;
	/*! @brief 分类多边形表
	 */
	std::vector<ClassifyEdgeTable>* m_classifyEdgeTables;
	/*! @brief 活化边表：紧凑数组，有效元素为 [0, m_aetSize)
	 *
	 *	原来是 unordered_map<int,ActiveEdgeTable>：每条扫描线都要把整张哈希表
	 *	遍历一遍（指针追逐），而且每次读改写都要把整个结构体拷进拷出。
	 *	改成连续数组后是顺序访存，且可以直接在数组元素上原地修改。
	 */
	std::vector<ActiveEdgeTable> m_aet;
	/*! @brief 面 id -> 该面在活化边表中的下标；-1 表示不在活化边表中
	 *
	 *	取代原来的 m_activeTriangles(unordered_set) 和
	 *	m_activeEdgeTables(unordered_map) 两张哈希表，降到一次数组下标访问
	 */
	std::vector<int> m_triSlot;
	/*! @brief 活化边表中当前有效的边对个数
	 */
	int m_aetSize;
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
	/*! @brief 缓冲区中是否已有渲染结果
	 *
	 *	用于判断同一个对象再次光栅化前是否需要清空缓冲，
	 *	保证"只渲染其中一种算法"和"依次渲染所有算法"结果一致
	 */
	bool m_dirty;
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