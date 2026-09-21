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
}

void Model::loadModel(std::string modelPath) {
	m_modelName = modelPath.substr(7, modelPath.length() - 11);
	float maxX = std::numeric_limits<float>::min(), minX = std::numeric_limits<float>::max();
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
			auto face = new int[3];
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
}


glm::vec3* Model::getFace(int i) const {
	glm::vec3* faceVertices = new glm::vec3[3];
	faceVertices[0] = m_vertices[m_faces[i][0]];
	faceVertices[1] = m_vertices[m_faces[i][1]];
	faceVertices[2] = m_vertices[m_faces[i][2]];
	return faceVertices;
}

glm::vec3* Model::getMVPFace(int i) const{
	glm::vec3* faceVertices = new glm::vec3[3];
	faceVertices[0] = m_mvpVertices[m_faces[i][0]];
	faceVertices[1] = m_mvpVertices[m_faces[i][1]];
	faceVertices[2] = m_mvpVertices[m_faces[i][2]];
	return faceVertices;
}

void Model::mvpTransform(Scene& scene) {
	m_mvpVertices = new glm::vec3[m_vertexNum];
	for (int i = 0; i < m_vertexNum; i++) {
		m_mvpVertices[i] = scene.mvpTransform(m_vertices[i]);
	}
}

void Model::calAxisParams() {
	m_axisCenters = new glm::vec3[m_faceNum];
	m_axisMaximums = new glm::vec3[m_faceNum];
	m_axisMinimums = new glm::vec3[m_faceNum];
	for (int i = 0; i < m_faceNum; i++) {
		m_axisCenters[i] = glm::vec3(1.0f);
		m_axisMaximums[i] = glm::vec3(1.0f);
		m_axisMinimums[i] = glm::vec3(1.0f);
	}
	for (int i = 0; i < m_faceNum; i++) {
		for (int j = 0; j < 3; j++){
			calAxisParam(i, j);
		}
	}
}

void Model::calAxisParam(int i, int axis) const{
	int* vertices = m_faces[i];
	float max, min;
	if (axis == 2) {
		max = glm::max(glm::abs(m_mvpVertices[vertices[0]].z), glm::max(glm::abs(m_mvpVertices[vertices[1]].z), glm::abs(m_mvpVertices[vertices[2]].z)));
		min = glm::min(glm::abs(m_mvpVertices[vertices[0]].z), glm::min(glm::abs(m_mvpVertices[vertices[1]].z), glm::abs(m_mvpVertices[vertices[2]].z)));
	}else {
		max = glm::max(m_mvpVertices[vertices[0]][axis], glm::max(m_mvpVertices[vertices[1]][axis], m_mvpVertices[vertices[2]][axis]));
		min = glm::min(m_mvpVertices[vertices[0]][axis], glm::min(m_mvpVertices[vertices[1]][axis], m_mvpVertices[vertices[2]][axis]));
	}
	m_axisMaximums[i][axis] = max;
	m_axisMinimums[i][axis] = min;
	m_axisCenters[i][axis] = (max + min) / 2;
}

