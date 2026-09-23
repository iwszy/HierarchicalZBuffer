#ifndef __OCCLUSION_SCENE_HPP__
#define __OCCLUSION_SCENE_HPP__

#include <array>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "model.hpp"

/*! @brief 生成用于验证层次 Z-Buffer 的"体素城市"高遮挡场景
 *
 *  与 tools/genOcclusionScene.py 生成的是同一个场景：M x M x M 个轴对齐立方体按格子
 *  排布，从相机看过去每条视线都要穿过 M 层立方体，depth complexity 约为 M。
 *  三角形数 = 12 * M^3，M = 28 时是 263 424，与 Crytek Sponza 同量级。
 *
 *  放在程序里生成、而不是提交 obj 文件：M=28 的场景有 11 MB，这里用到的四种组合加起来
 *  31 MB，而生成它的代码只有几十行。顺带还能随手切换"近→远 / 远→近"的三角形顺序 ——
 *  这正是层次 Z-Buffer 收益的前提，本身就值得对比。
 *
 *  @param[in,out] model: 生成到的模型
 *  @param[in] gridSize: 每边的格子数 M
 *  @param[in] nearToFar: true 表示先输出靠近相机的层（相机在 +z 方向）
 *  @param[in] cellRatio: 立方体边长占格子的比例。大于 1 时相邻立方体互相重叠、
 *             屏幕上不留缝；小于 1 会留缝，透过缝隙能看到远处几何，深度金字塔的
 *             "区域最大深度"会被背景拉高，遮挡剔除率明显下降 —— 这本身就是一个
 *             值得观察的现象。
 *  @param[in] name: 模型名（用于输出文件名）
 */
inline void buildOcclusionScene(Model& model, int gridSize, bool nearToFar, float cellRatio,
                                const std::string& name) {
	//立方体 8 个顶点的局部编号: index = dz*4 + dy*2 + dx，dz/dy/dx 取 0(负) 或 1(正)
	//下面 12 个三角形都按"从外侧看逆时针"给出，保证法线朝外
	static const int kFaces[12][3] = {
		{ 1, 3, 7 }, { 1, 7, 5 },   //+X
		{ 0, 4, 6 }, { 0, 6, 2 },   //-X
		{ 2, 6, 7 }, { 2, 7, 3 },   //+Y
		{ 0, 1, 5 }, { 0, 5, 4 },   //-Y
		{ 4, 5, 7 }, { 4, 7, 6 },   //+Z
		{ 0, 2, 3 }, { 0, 3, 1 },   //-Z
	};
	const float spacing = 2.0f / static_cast<float>(gridSize);
	const float half = spacing * cellRatio * 0.5f;
	const int cubeCount = gridSize * gridSize * gridSize;

	std::vector<glm::vec3> vertices;
	std::vector<std::array<int, 3>> faces;
	vertices.reserve(cubeCount * 8);
	faces.reserve(cubeCount * 12);

	for (int layer = 0; layer < gridSize; layer++) {
		const int k = nearToFar ? (gridSize - 1 - layer) : layer;
		for (int j = 0; j < gridSize; j++) {
			for (int i = 0; i < gridSize; i++) {
				const glm::vec3 center(-1.0f + (2 * i + 1) / static_cast<float>(gridSize),
				                       -1.0f + (2 * j + 1) / static_cast<float>(gridSize),
				                       -1.0f + (2 * k + 1) / static_cast<float>(gridSize));
				const int base = static_cast<int>(vertices.size());
				for (int dz = -1; dz <= 1; dz += 2) {
					for (int dy = -1; dy <= 1; dy += 2) {
						for (int dx = -1; dx <= 1; dx += 2) {
							vertices.push_back(center + glm::vec3(dx * half, dy * half, dz * half));
						}
					}
				}
				for (int t = 0; t < 12; t++) {
					faces.push_back({ base + kFaces[t][0], base + kFaces[t][1], base + kFaces[t][2] });
				}
			}
		}
	}
	model.buildModel(vertices, faces, name);
}

#endif // !__OCCLUSION_SCENE_HPP__
