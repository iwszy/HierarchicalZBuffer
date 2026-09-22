#include "scene.hpp"

Scene::Scene() : m_camera(glm::vec3(0, 0, 1.8)), m_eyeDirection(glm::vec3(0, 0, -1)),
m_up(glm::vec3(0, 1, 0)),  m_lightDirection(glm::vec3(0, 0, 1)), m_near(-0.3f), m_far(-100), m_fov(90),
m_diffuseColor(glm::vec3(0.5, 0.5, 0.5)), m_width(1024), m_height(1024)
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
    return RView * TView;
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