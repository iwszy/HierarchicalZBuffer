#include "scene.hpp"

Scene::Scene() : Scene(Config{}) {}

Scene::Scene(const Config& config)
	: m_camera(config.camera), m_eyeDirection(config.eyeDirection), m_up(config.up),
	  m_lightDirection(config.lightDirection), m_near(config.nearPlane), m_far(config.farPlane),
	  m_fov(config.fov), m_ambient(config.ambient), m_diffuseColor(config.diffuseColor),
	  m_width(config.width), m_height(config.height)
{
    m_mvp = getView() * getProjection() * getViewport();
    m_lightDirection = glm::normalize(m_lightDirection);
}

glm::vec3 Scene::mvpTransform(const glm::vec3& vec) const{
    //将向量转为齐次坐标后再变换再转为真实坐标
    glm::vec4 vec1 = glm::vec4(vec, 1.0f) * m_mvp;
    return { vec1.x / vec1.w, vec1.y / vec1.w, vec1.z / vec1.w };
}


glm::mat4 Scene::getView() const{
    glm::vec3 w = -glm::normalize(m_eyeDirection);
    glm::vec3 u = glm::normalize(glm::cross(m_up, w));
    glm::vec3 v = glm::normalize(glm::cross(w, u));
    glm::mat4 TView(1.0f);
    glm::mat4 RView(1.0f);
    for (int i = 0; i < 3; i++) {
        TView[i][3] = -m_camera[i];
        RView[0][i] = u[i];
        RView[1][i] = v[i];
        RView[2][i] = w[i];
    }
    //行向量约定下，点的变换是 p * (TView * RView) = (p - camera) * RView：
    //必须【先平移、再旋转】。写成 RView * TView 就变成了 p * RView - camera，
    //即先旋转、再在世界坐标里减相机位置 —— 只要相机不在原点且朝向不是 (0,0,-1)，
    //旋转矩阵就不是单位阵，两者结果不同：相机沿 x/z 移动时物体不会正确地变近/变远。
    return TView * RView;
}

glm::mat4 Scene::getProjection() const {
    float top = glm::tan(m_fov * PI / 360) * glm::abs(m_near);
    float right = static_cast<float>(m_width) / static_cast<float>(m_height) * top;
    glm::mat4 projection(1.0f);
    projection[0][0] = m_near / right;
    projection[1][1] = m_near / top;
    projection[2][2] = (m_far + m_near) / (m_near - m_far);
    projection[2][3] = (2 * m_far * m_near) / (m_far - m_near);
    projection[3][2] = 1;
    projection[3][3] = 0;
    return projection;
}

glm::mat4 Scene::getViewport() const {
    glm::mat4 viewport(1.0f);
    viewport[0][0] = m_width / 2;
    viewport[1][1] = m_height / 2;
    viewport[0][3] = m_width / 2;
    viewport[1][3] = m_height / 2;
    return viewport;
}
