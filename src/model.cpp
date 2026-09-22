#include "model.hpp"
#include <limits>
#include <fstream>

Model::Model() {
	m_vertexNum = 0;
	m_faceNum = 0;
	m_mvpVertices = nullptr;
	m_axisCenters = nullptr;
	m_axisMaximums = nullptr;
	m_axisMinimums = nullptr;
	m_objectAxisCenters = nullptr;
	m_objectAxisMaximums = nullptr;
	m_objectAxisMinimums = nullptr;
	m_objectCentroids = nullptr;
	m_objectZMin = nullptr;
	m_objectZMax = nullptr;
}

void Model::loadModel(std::string modelPath) {
	m_modelName = modelPath.substr(7, modelPath.length() - 11);
	float maxX = std::numeric_limits<float>::lowest(), minX = std::numeric_limits<float>::max();
	float maxY = maxX, minY = minX, maxZ = maxX, minZ = minX, maxData = maxX;
	std::ifstream f;
	f.open(modelPath, std::ios::in);
	std::string line;
	while (std::getline(f, line)) {
		std::string start = line.substr(0, 2);
		if (start == "v ") {
			std::string num("");
			glm::vec3 vertex(0, 0, 0);
			int vIndex = 0, startIndex = 1;
			while (line[startIndex] == ' ') {
				startIndex++;
			}
			for (int i = startIndex; i < line.size(); i++) {
				if (line[i] == ' ') {
					vertex[vIndex] = std::stof(num);
					num = "";
					vIndex++;
				} else {
					num += line[i];
				}
			}
			vertex[vIndex] = std::stof(num);
			maxX = glm::max(maxX, vertex.x); minX = glm::min(minX, vertex.x);
			maxY = glm::max(maxY, vertex.y); minY = glm::min(minY, vertex.y);
			maxZ = glm::max(maxZ, vertex.z); minZ = glm::min(minZ, vertex.z);
			m_vertices.push_back(vertex);
			m_vertexNum++;
		} else if (start == "f ") {
			std::string index("");
			bool isBegin = true, haveSplit = false;
			std::array<int, 3> face{};
			int faceIndex = 0;
			for (int i = 2; i < line.size(); i++) {
				if (isBegin && line[i] == '/') {
					face[faceIndex++] = std::stoi(index) - 1;
					isBegin = false;
					index = "";
					haveSplit = true;
				} else if (line[i] == ' ') {
					if (isBegin) {
						face[faceIndex++] = std::stoi(index) - 1;
						index = "";
					}
					isBegin = true;
				} else if (isBegin) {
					index += line[i];
				}
			}
			if (!haveSplit) {
				face[faceIndex++] = std::stoi(index) - 1;
			}
			m_faces.push_back(face);
			m_faceNum++;
		}
	}
	//将模型调整至中间位置
	float midX = (maxX + minX) / 2, midY = (maxY + minY) / 2, midZ = (maxZ + minZ) / 2;
	for (auto it = m_vertices.begin(); it != m_vertices.end(); ++it) {
		it->x -= midX;
		it->y -= midY;
		it->z -= midZ;
		maxData = glm::max(glm::max(maxData, glm::abs(it->x)), glm::max(glm::abs(it->y), glm::abs(it->z)));
	}
	//将模型的坐标调整至[-1,1]^3
	for (auto it = m_vertices.begin(); it != m_vertices.end(); ++it) {
		it->x /= maxData;
		it->y /= maxData;
		it->z /= maxData;
	}
}

Model::~Model() {
	delete[] m_mvpVertices;
	delete[] m_axisCenters;
	delete[] m_axisMaximums;
	delete[] m_axisMinimums;
	delete[] m_objectAxisCenters;
	delete[] m_objectAxisMaximums;
	delete[] m_objectAxisMinimums;
	delete[] m_objectCentroids;
	delete[] m_objectZMin;
	delete[] m_objectZMax;
}


void Model::getFace(int i, glm::vec3 face[3]) const {
	face[0] = m_vertices[m_faces[i][0]];
	face[1] = m_vertices[m_faces[i][1]];
	face[2] = m_vertices[m_faces[i][2]];
}

void Model::getMVPFace(int i, glm::vec3 face[3]) const {
	face[0] = m_mvpVertices[m_faces[i][0]];
	face[1] = m_mvpVertices[m_faces[i][1]];
	face[2] = m_mvpVertices[m_faces[i][2]];
}

void Model::mvpTransform(Scene& scene) {
	delete[] m_mvpVertices;
	m_mvpVertices = new glm::vec3[m_vertexNum];
	for (int i = 0; i < m_vertexNum; i++) {
		m_mvpVertices[i] = scene.mvpTransform(m_vertices[i]);
	}
}

void Model::calAxisParamsImpl(const glm::vec3* verts, glm::vec3* centers, glm::vec3* maximums, glm::vec3* minimums) const {
	for (int i = 0; i < m_faceNum; i++) {
		const std::array<int, 3>& idx = m_faces[i];
		for (int axis = 0; axis < 3; axis++) {
			float min, max;
			if (axis == 2) {
				//z 轴取绝对值：深度缓冲区里用的是 |z|，越远值越大
				max = glm::max(glm::abs(verts[idx[0]].z), glm::max(glm::abs(verts[idx[1]].z), glm::abs(verts[idx[2]].z)));
				min = glm::min(glm::abs(verts[idx[0]].z), glm::min(glm::abs(verts[idx[1]].z), glm::abs(verts[idx[2]].z)));
			} else {
				max = glm::max(verts[idx[0]][axis], glm::max(verts[idx[1]][axis], verts[idx[2]][axis]));
				min = glm::min(verts[idx[0]][axis], glm::min(verts[idx[1]][axis], verts[idx[2]][axis]));
			}
			maximums[i][axis] = max;
			minimums[i][axis] = min;
			centers[i][axis] = (max + min) / 2;
		}
	}
}

void Model::calAxisParams() {
	delete[] m_axisCenters;
	delete[] m_axisMaximums;
	delete[] m_axisMinimums;
	m_axisCenters = new glm::vec3[m_faceNum];
	m_axisMaximums = new glm::vec3[m_faceNum];
	m_axisMinimums = new glm::vec3[m_faceNum];
	calAxisParamsImpl(m_mvpVertices, m_axisCenters, m_axisMaximums, m_axisMinimums);
}

void Model::calAxisParamsObject() {
	delete[] m_objectAxisCenters;
	delete[] m_objectAxisMaximums;
	delete[] m_objectAxisMinimums;
	m_objectAxisCenters = new glm::vec3[m_faceNum];
	m_objectAxisMaximums = new glm::vec3[m_faceNum];
	m_objectAxisMinimums = new glm::vec3[m_faceNum];
	calAxisParamsImpl(m_vertices.data(), m_objectAxisCenters, m_objectAxisMaximums, m_objectAxisMinimums);
	//真实中心：z 轴不取绝对值，供八叉树做三维空间划分
	delete[] m_objectCentroids;
	m_objectCentroids = new glm::vec3[m_faceNum];
	delete[] m_objectZMin;
	delete[] m_objectZMax;
	m_objectZMin = new float[m_faceNum];
	m_objectZMax = new float[m_faceNum];
	for (int i = 0; i < m_faceNum; i++) {
		const std::array<int, 3>& idx = m_faces[i];
		m_objectCentroids[i] = (m_vertices[idx[0]] + m_vertices[idx[1]] + m_vertices[idx[2]]) / 3.0f;
		m_objectZMin[i] = glm::min(m_vertices[idx[0]].z, glm::min(m_vertices[idx[1]].z, m_vertices[idx[2]].z));
		m_objectZMax[i] = glm::max(m_vertices[idx[0]].z, glm::max(m_vertices[idx[1]].z, m_vertices[idx[2]].z));
	}
}