#include "scanlineZBuffer.hpp"
#include <iostream>
#include "stb_image.hpp"
#include "stb_image_write.hpp"
#include "triangleRasterizer.hpp"

ScanlineZBuffer::ScanlineZBuffer(int width, int height) {
	m_width = width;
	m_height = height;
	m_dirty = false;
	int pixelNum = m_width * m_height;
	m_zBuffer = new float[pixelNum];
	m_image = new unsigned char[pixelNum * 4];
	for (int i = 0; i < pixelNum; i++) {
		m_zBuffer[i] = std::numeric_limits<float>::max();
		m_image[i * 4] = 0;
		m_image[i * 4 + 1] = 0;
		m_image[i * 4 + 2] = 0;
		m_image[i * 4 + 3] = 255;
	}
	m_classifyEdgeTables = new std::vector<ClassifyEdgeTable>[m_height];
	m_aetSize = 0;
	mode = 0;
}

ScanlineZBuffer::~ScanlineZBuffer() {
	delete[] m_zBuffer;
	delete[] m_image;
	delete[] m_classifyEdgeTables;
}

void ScanlineZBuffer::showInfo() const {
	if (mode == 1) {
		std::cout << "特化ScanlineZBuffer: " << "渲染时间为" << m_renderTime.count() << "ms" << std::endl;
	}else {
		std::cout << "经典ScanlineZBuffer: 建表时间为" << m_buildTableTime.count() << "ms, " << "渲染时间为" << m_renderTime.count() << "ms, " << "总时间为" << (m_buildTableTime + m_renderTime).count() << "ms" << std::endl;
	}
}

void ScanlineZBuffer::render() const {
	stbi_flip_vertically_on_write(1);
	std::string modelName = m_modelName;
	if (mode == 1) {
		stbi_write_png(modelName.insert(0, "results/SpecialScanlineZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
	}else {
		stbi_write_png(modelName.insert(0, "results/ClassicScanlineZBuffer_").append(".png").c_str(), m_width, m_height, 4, m_image, 0);
	}
}

void ScanlineZBuffer::rasterizeScene(Model& model, Scene& scene) {
	m_modelName = model.getModelName();
	//清空颜色缓冲与 z-Buffer，使同一个对象可以被安全地重复使用
	if (m_dirty) {
		const int pixelNum = m_width * m_height;
		for (int i = 0; i < pixelNum; i++) {
			m_zBuffer[i] = std::numeric_limits<float>::max();
			m_image[i * 4] = m_image[i * 4 + 1] = m_image[i * 4 + 2] = 0;
		}
	}
	m_dirty = true;
	//选择不同的渲染模式可根据不同方法进行渲染
	//1表示为特化扫描线算法，其余为经典扫描线算法
	if (mode == 1) {
		auto start = std::chrono::steady_clock::now();
		int faceNum = model.getFaceNum();
		model.mvpTransform(scene);
		glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
		const float ambient = scene.getAmbient();
		glm::vec3 face[3];
		for (int i = 0; i < faceNum; i++) {
			model.getFace(i, face);
			glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
			float diffuseIntensity = ambient + (1.0f - ambient) * glm::max(0.f, glm::dot(normal, lightDirection));
			model.getMVPFace(i, face);
			for (int j = 0; j < 3; j++) {
				face[j].y = static_cast<float>(static_cast<int>(face[j].y));
			}
			rasterizeTriangle(face, diffuseColor * diffuseIntensity);
		}
		auto end = std::chrono::steady_clock::now();
		m_renderTime = end - start;
		return;
	}
	auto start = std::chrono::steady_clock::now();
	generateTables(model, scene);
	auto end = std::chrono::steady_clock::now();
	m_buildTableTime = end - start;
	start = std::chrono::steady_clock::now();
	for (int i = m_height - 1; i >= 0; i--) {
		//事件处理：把本行新出现的边并入活化边表
		const std::vector<ClassifyEdgeTable>& edgeBucket = m_classifyEdgeTables[i];
		for (size_t j = 0; j < edgeBucket.size(); j++) {
			const ClassifyEdgeTable& classifyEdge = edgeBucket[j];
			int id = classifyEdge.id;
			int slot = m_triSlot[id];
			if (slot < 0) {
				//当该边对应的三角形是首次出现，则统一将该边视为左侧边，同时将dyRight赋值为-1以标识该活化边对只有1条边
				slot = m_aetSize++;
				m_triSlot[id] = slot;
				m_aet[slot] = { classifyEdge.x, 0.0f, classifyEdge.dx, 0.0f, classifyEdge.dy, -1, classifyEdge.x, classifyEdge.z, i, m_triangles[id].dzx, m_triangles[id].dzy, id};
			} else {
				//当该边对应的三角形不是首次出现，则进行以下判断
				//dyRight != -1 => 该边对有2条边，此时需判断是否有1条边扫描完毕，如果有，则将该边填充至扫描完毕的那条边
				//dyRight == -1 => 该边对只有1条边，此时需进入下一层判断
				//该边所在的三角形是上平底 => 根据该边上顶点与已有边的上顶点的位置填充边对
				//该边所在的三角形不是上平底 => 根据该边x增量与已有边的x增量的大小关系填充边对
				ActiveEdgeTable& activeEdge = m_aet[slot];
				if (activeEdge.dyRight == -1) {
					//此时该边对只有一条边，需要判断新边在左还是在右。
					//两条边必然共用一个顶点，分两种构型：
					//  · 共用上顶点（普通三角形）：两条边此刻的 x 是同一个浮点值、完全相同，
					//    比 x 没有意义，只能靠每扫描行的 x 增量 dx 判断，dx 小者向左展开；
					//  · 共用下顶点（上平底三角形）：两条边此刻 x 不同，而且 dx 的大小关系
					//    与左右相反（起点越靠右，收敛到底顶点所需的 dx 越小），必须直接比 x。
					//注意：这里不能用"一律比 dx"代替，实测那样做会让 6 个模型最多 6.2% 的像素出错。
					const bool newEdgeOnLeft = (glm::abs(classifyEdge.x - activeEdge.xLeft) < EPSILON)
						? (classifyEdge.dx < activeEdge.dxLeft)
						: (classifyEdge.x < activeEdge.xLeft);
					if (newEdgeOnLeft) {
						//新边在左：原来那条边让到右侧
						activeEdge.xRight = activeEdge.xLeft;
						activeEdge.dxRight = activeEdge.dxLeft;
						activeEdge.dyRight = activeEdge.dyLeft;
						activeEdge.xLeft = classifyEdge.x;
						activeEdge.dxLeft = classifyEdge.dx;
						activeEdge.dyLeft = classifyEdge.dy;
						activeEdge.xRef = classifyEdge.x;
						activeEdge.zRef = classifyEdge.z;
						activeEdge.yRef = i;
					} else {
						//新边在右
						activeEdge.xRight = classifyEdge.x;
						activeEdge.dxRight = classifyEdge.dx;
						activeEdge.dyRight = classifyEdge.dy;
					}
				} else if (activeEdge.dyRight == 0) {
					activeEdge.xRight = classifyEdge.x;
					activeEdge.dxRight = classifyEdge.dx;
					activeEdge.dyRight = classifyEdge.dy;
				} else if (activeEdge.dyLeft == 0) {
					activeEdge.xLeft = classifyEdge.x;
					activeEdge.dxLeft = classifyEdge.dx;
					activeEdge.dyLeft = classifyEdge.dy;
					activeEdge.xRef = classifyEdge.x;
					activeEdge.zRef = classifyEdge.z;
					activeEdge.yRef = i;
				}
			}
		}
		//光栅化本行：顺序遍历紧凑数组，边对就地修改
		for (int s = 0; s < m_aetSize; s++) {
			ActiveEdgeTable& edge = m_aet[s];
			int id = edge.id;
			const glm::vec3& color = m_triangles[id].color;
			//当识别到该边对只有1条边，表示该边对应的三角形只跨越1条扫描线，此时直接从左向右扫描三角形
			const float spanLeft = edge.xLeft;
			const float spanRight = (edge.dyRight == -1) ? (edge.xLeft + edge.dxLeft) : edge.xRight;
			int xLeft = static_cast<int>(spanLeft), xRight = static_cast<int>(spanRight);
			//把扫描线裁剪到渲染窗口内，避免越界写入 m_zBuffer / m_image
			if (xLeft < 0) {
				xLeft = 0;
			}
			if (xRight >= m_width) {
				xRight = m_width - 1;
			}
			//本行的深度由平面方程直接求值（不再逐像素累加）；求值位置还要夹回该扫描行的
			//真实跨度之内：像素范围是向零截断得到的，最左那个像素的整数坐标可能落在跨度
			//之外最多 1 像素，而窄长三角形的 dzx 可以很大，外推出去深度会被甩出视锥
			//（实测最大越界幅度到过 36.6，而整个视锥才 [-1, 1]）
			const float rowZ = edge.zRef + (edge.yRef - i) * edge.dzy;
			for (int j = xLeft; j <= xRight; j++) {
				const float xc = (j < spanLeft) ? spanLeft : ((static_cast<float>(j) > spanRight) ? spanRight : static_cast<float>(j));
				//深度取 -z（越远越大），与光栅化内核保持一致
				const float depth = -(rowZ + (xc - edge.xRef) * edge.dzx);
				int index = m_width * i + j;
				if (m_zBuffer[index] < depth) {
					continue;
				}
				m_zBuffer[index] = depth;
				m_image[index * 4] = static_cast<unsigned char>(color.r * 255);
				m_image[index * 4 + 1] = static_cast<unsigned char>(color.g * 255);
				m_image[index * 4 + 2] = static_cast<unsigned char>(color.b * 255);
			}
			edge.dyLeft--;
			edge.dyRight--;
			//当三角形扫描完毕，先标记、稍后统一压缩，避免打乱其余边对的相对顺序
			if ((edge.dyLeft == 0 && edge.dyRight == 0) || edge.dyRight == -2) {
				m_triSlot[id] = -1;
				edge.id = -1;
				continue;
			}
			edge.xLeft += edge.dxLeft;
			edge.xRight += edge.dxRight;
		}
		//稳定压缩：保持边对相对顺序不变，使同深度像素的覆盖顺序与原实现一致
		int alive = 0;
		for (int s = 0; s < m_aetSize; s++) {
			if (m_aet[s].id < 0) {
				continue;
			}
			if (alive != s) {
				m_aet[alive] = m_aet[s];
				m_triSlot[m_aet[alive].id] = alive;
			}
			alive++;
		}
		m_aetSize = alive;
	}
	end = std::chrono::steady_clock::now();
	m_renderTime = end - start;
}

void ScanlineZBuffer::generateTables(Model& model, Scene& scene) {
	model.mvpTransform(scene);
	int faceNum = model.getFaceNum();

	//每次重建都清空上一次留下的状态，使本方法可以重复调用
	for (int y = 0; y < m_height; y++) {
		m_classifyEdgeTables[y].clear();
	}
	m_aetSize = 0;
	m_triSlot.assign(faceNum, -1);
	if (static_cast<int>(m_aet.size()) < faceNum) {
		m_aet.resize(faceNum);
	}
	m_triangles.resize(faceNum);
	float faceDiff[3];
	glm::vec3 lightDirection = scene.getLightDirection(), diffuseColor = scene.getDiffuseColor();
	const float ambient = scene.getAmbient();
	glm::vec3 face[3];
	for (int i = 0; i < faceNum; i++) {
		model.getFace(i, face);
		glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
		float diffuseIntensity = ambient + (1.0f - ambient) * glm::max(0.f, glm::dot(normal, lightDirection));
		model.getMVPFace(i, face);
		for (int j = 0; j < 3; j++) {
			face[j].y = static_cast<float>(static_cast<int>(face[j].y));
		}
		for (int j = 0; j < 3; j++) {
			faceDiff[j] = (face[j].y - face[(j + 1) % 3].y);
		}
		//由于使用的是变换后的z值，故使用变换后的三角形计算所在平面的系数
		float a = faceDiff[0] * (face[0].z - face[2].z) - (face[1].z - face[0].z) * faceDiff[2];
		float b = (face[1].z - face[0].z) * (face[2].x - face[0].x) - (face[1].x - face[0].x) * (face[2].z - face[0].z);
		float c = (face[1].x - face[0].x) * faceDiff[2] + faceDiff[0] * (face[2].x - face[0].x);
		float dzx = -a / c, dzy = b / c;
		//当三角形面积为0时，c=0，此时需要转为线的方式计算dzx与dzy
		if (glm::abs(c) < EPSILON) {
			int minXIndex = 0, maxXIndex = 0, minYIndex = 0, maxYIndex = 0;
			float minX = face[0].x, maxX = minX, maxY = face[0].y, minY = maxY;
			for (int j = 1; j < 3; j++) {
				if (minX > face[j].x) {
					minX = face[j].x;
					minXIndex = j;
				}
				if (maxX < face[j].x) {
					maxX = face[j].x;
					maxXIndex = j;
				}
				if (minY > face[j].y) {
					minY = face[j].y;
					minYIndex = j;
				}
				if (maxY < face[j].y) {
					maxY = face[j].y;
					maxYIndex = j;
				}
			}
			dzx = (face[maxXIndex].z - face[minXIndex].z) / (maxX - minX);
			dzy = (face[maxYIndex].z - face[minYIndex].z) / (maxY - minY);
		}
		//屏幕面积过小的退化三角形直接跳过，理由见 triangleRasterizer.hpp 的说明
		if (glm::abs(c) < 2.0f * 0.01f) {
			continue;
		}
		m_triangles[i] = { dzx, dzy, diffuseColor * diffuseIntensity };
		//当三角形三个顶点y值相同时，经典扫描线算法会不渲染这个三角形，此时需要特殊处理
		//将该三角形跨越的x值作为dx以在渲染时特殊处理
		if (glm::abs(faceDiff[0]) < EPSILON && glm::abs(faceDiff[1]) < EPSILON) {
			int minIndex = 0;
			float minX = face[0].x, maxX = minX;
			for (int j = 1; j < 3; j++) {
				if (minX > face[j].x) {
					minX = face[j].x;
					minIndex = j;
				}
				if (maxX < face[j].x) {
					maxX = face[j].x;
				}
			}
			int flatY = static_cast<int>(face[0].y);
			if (flatY >= 0 && flatY < m_height) {
			    m_classifyEdgeTables[flatY].push_back({minX, maxX - minX, 1, face[minIndex].z,i});
			}
			continue;
		}
		for (int j = 0; j < 3; j++) {
			int nextJ = (j + 1) % 3, preJ = (j + 2) % 3, curJ = j;
			if (glm::abs(faceDiff[j]) < EPSILON) {
				continue;
			}
			float dx = (face[nextJ].x - face[j].x) / faceDiff[j];
			//若face[j]不是上顶点，则交换curJ与nextJ的值。同时当上顶点为非极值点时，将其往下一行
			if (faceDiff[j] < 0) {
				std::swap(curJ, nextJ);
			}
			float x = face[curJ].x;
			int ymax = static_cast<int>(face[curJ].y), ymin = static_cast<int>(face[nextJ].y);
			if ((face[curJ].y - face[nextJ].y) * (face[curJ].y - face[preJ].y) < -EPSILON) {
				ymax--;
				x += dx;
			}
			//把边裁剪到渲染窗口内：起始扫描线在窗口上方时，把 x 的插值推进过去
			if (ymax >= m_height) {
			    int skip = ymax - (m_height - 1);
			    x += dx * skip;
			    ymax = m_height - 1;
			}
			if (ymax >= 0) {
			    //记录该边进入扫描线时的参考点 (x, ymax)，以及深度平面在该点的取值。
			    //渲染时用平面方程直接求值，不再沿扫描线逐行累加
			    const float zRef = face[curJ].z + (face[curJ].y - ymax) * dzy + (x - face[curJ].x) * dzx;
			    m_classifyEdgeTables[ymax].push_back({ x, dx, ymax - ymin + 1, zRef, i });
			}
		}
	}
}

void ScanlineZBuffer::rasterizeTriangle(glm::vec3* face, glm::vec3 color) {
    rasterizeTriangleScanline(face, color, m_image, m_width, m_height,
        [this](int x, int y, float depth) {
            //普通 z-Buffer：只做深度测试并写 z-Buffer
            const int index = y * m_width + x;
            if (m_zBuffer[index] < depth) {
                return false;
            }
            m_zBuffer[index] = depth;
            return true;
        });
}