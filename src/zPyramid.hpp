#ifndef __ZPYRAMID_HPP__
#define __ZPYRAMID_HPP__

#include <vector>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>

/*! @brief 空区域的深度值（即"这里还没有任何几何"） */
constexpr float emptyDepth = std::numeric_limits<float>::max();
/*! @brief 与各算法头文件里 EPSILON 宏取值一致的常量 */
constexpr double pyramidEpsilon = 1e-5;

/*! @brief 扁平 Z-max 金字塔 —— 层次 Z-Buffer 的深度层次结构
 *
 *  用 mipmap 式的连续数组代替指针四叉树：
 *
 *      第 0 层   逐像素，尺寸 width x height
 *      第 L 层   每个单元覆盖 2^L x 2^L 像素，存该区域内的【最大深度】
 *      父单元    第 L 层单元 (x,y) 的父单元是第 L+1 层的 (x>>1, y>>1)
 *      最粗层    只剩下 1x1
 *
 *  总单元数约 4/3 * width * height（1024x1024 时是 1398101 个 float 约 5.6 MB），
 *  全部连续存储、没有指针，因此既省内存又对缓存友好；
 *  而"建树"退化成一次内存填充（因为一开始所有区域都没有几何，深度都是最大值）。
 *
 *  正确性依据：若某区域的【最大深度】小于三角形的【最小深度】，
 *  则该区域里任何一个片元都会被已有几何的 z-test 拒绝，可以整块跳过。
 *  这个判据是保守的——不满足时可能白跑，但绝不会错误剔除。
 */
class ZPyramid {
public:
	ZPyramid(int width, int height) : m_width(width), m_height(height) {
		//逐层折半，直到最粗的一层只剩下 1x1
		int w = width, h = height;
		while (true) {
			m_levelWidth.push_back(w);
			m_levelHeight.push_back(h);
			if (w == 1 && h == 1) {
				break;
			}
			w = (w + 1) / 2;
			h = (h + 1) / 2;
		}
		const int levelNum = static_cast<int>(m_levelWidth.size());
		m_offset.resize(levelNum);
		int total = 0;
		for (int l = 0; l < levelNum; l++) {
			m_offset[l] = total;
			total += m_levelWidth[l] * m_levelHeight[l];
		}
		m_data.assign(total, emptyDepth);
	}

	ZPyramid(const ZPyramid&) = delete;
	ZPyramid& operator=(const ZPyramid&) = delete;

	/*! @brief 把所有层重置为"没有几何"，使同一个对象可以重复使用 */
	void clear() {
		std::fill(m_data.begin(), m_data.end(), emptyDepth);
	}

	int levelNum() const { return static_cast<int>(m_offset.size()); }
	int rootLevel() const { return levelNum() - 1; }
	int levelWidth(int level) const { return m_levelWidth[level]; }
	int levelHeight(int level) const { return m_levelHeight[level]; }
	int width() const { return m_width; }
	int height() const { return m_height; }
	size_t cellNum() const { return m_data.size(); }
	size_t memoryBytes() const {
		return m_data.size() * sizeof(float) + (m_levelWidth.size() + m_levelHeight.size() + m_offset.size()) * sizeof(int);
	}

	/*! @brief 读取某个像素（第 0 层）的深度 */
	float pixelDepth(int x, int y) const { return m_data[static_cast<size_t>(y) * m_width + x]; }

	/*! @brief 读取某层某个单元的深度 */
	float depthAt(int level, int x, int y) const {
		return m_data[static_cast<size_t>(m_offset[level]) + static_cast<size_t>(y) * m_levelWidth[level] + x];
	}

	//单元在像素空间的四条边（right / top 为边界 + 1）
	int nodeLeft(int level, int x) const { return x << level; }
	int nodeRight(int level, int x) const { return std::min((x + 1) << level, m_width); }
	int nodeBottom(int level, int y) const { return y << level; }
	int nodeTop(int level, int y) const { return std::min((y + 1) << level, m_height); }

	/*! @brief 该单元是否完全包含给定的包围盒 */
	bool nodeContains(int level, int x, int y, float xmin, float xmax, float ymin, float ymax) const {
		return nodeLeft(level, x) - xmin <= pyramidEpsilon
			&& nodeRight(level, x) - xmax >= -pyramidEpsilon
			&& nodeBottom(level, y) - ymin <= pyramidEpsilon
			&& nodeTop(level, y) - ymax >= -pyramidEpsilon;
	}

	/*! @brief 写入一个像素的深度，并沿父链向上更新区域最大深度
	 *
	 *  与原来四叉树的 update() 语义完全一致：只有在某一层的值真的变小了才继续往上走，
	 *  否则上面几层也不会变，直接停。
	 */
	void writePixel(int x, int y, float depth) {
		m_data[static_cast<size_t>(y) * m_width + x] = depth;
		int cx = x >> 1, cy = y >> 1;
		for (int level = 1; level < levelNum(); level++) {
			const int childW = m_levelWidth[level - 1];
			const int childH = m_levelHeight[level - 1];
			const size_t childOff = static_cast<size_t>(m_offset[level - 1]);
			float maxDepth = std::numeric_limits<float>::lowest();
			for (int dy = 0; dy < 2; dy++) {
				const int yy = (cy << 1) + dy;
				if (yy >= childH) {
					break;
				}
				for (int dx = 0; dx < 2; dx++) {
					const int xx = (cx << 1) + dx;
					if (xx >= childW) {
						break;
					}
					maxDepth = std::max(maxDepth, m_data[childOff + static_cast<size_t>(yy) * childW + xx]);
				}
			}
			const size_t idx = static_cast<size_t>(m_offset[level]) + static_cast<size_t>(cy) * m_levelWidth[level] + cx;
			if (m_data[idx] - maxDepth > pyramidEpsilon) {
				m_data[idx] = maxDepth;
			} else {
				break;
			}
			cx >>= 1;
			cy >>= 1;
		}
	}

	/*! @brief 判断三角形是否需要光栅化（是否有可能被已有几何遮挡）
	 *
	 *  取出三个顶点所在的 0 层单元（即像素），找它们在金字塔中的最低公共祖先
	 *  （也就是能覆盖整个三角形的最小单元），再比较三角形最小深度与该单元的最大深度。
	 *  原来用指针上溯找公共祖先，这里只需要比较三个像素坐标右移若干位之后是否相等。
	 */
	bool isNeedRasterize(const glm::vec3* vertices) const {
		//顶点可能落在窗口外，先夹取到窗口内
		int vx[3], vy[3];
		for (int i = 0; i < 3; i++) {
			vx[i] = glm::clamp(static_cast<int>(vertices[i].x), 0, m_width - 1);
			vy[i] = glm::clamp(static_cast<int>(vertices[i].y), 0, m_height - 1);
		}
		int level = 0;
		const int top = rootLevel();
		while (level < top) {
			const int x0 = vx[0] >> level, x1 = vx[1] >> level, x2 = vx[2] >> level;
			const int y0 = vy[0] >> level, y1 = vy[1] >> level, y2 = vy[2] >> level;
			if (x0 == x1 && x1 == x2 && y0 == y1 && y1 == y2) {
				break;
			}
			level++;
		}
		const float z = glm::min(glm::abs(vertices[0].z), glm::min(glm::abs(vertices[1].z), glm::abs(vertices[2].z)));
		return z < depthAt(level, vx[0] >> level, vy[0] >> level);
	}

private:
	/*! @brief 所有层的深度，按层依次排列 */
	std::vector<float> m_data;
	/*! @brief 每层在 m_data 中的起始下标 */
	std::vector<int> m_offset;
	/*! @brief 每层的宽、高（单元数） */
	std::vector<int> m_levelWidth, m_levelHeight;
	int m_width, m_height;
};

#endif // !__ZPYRAMID_HPP__