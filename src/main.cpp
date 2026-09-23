#include <iostream>
#include <string>
#include "model.hpp"
#include "scene.hpp"
#include "basicZBuffer.hpp"
#include "scanlineZBuffer.hpp"
#include "basicHierarchicalZBuffer.hpp"
#include "hierarchicalZBuffer.hpp"

/*! @brief 用六种算法依次渲染一个模型
 *  @param[in,out] model: 模型
 *  @param[in] scene: 场景
 */
static void renderAll(Model& model, Scene& scene) {
    std::cout << "当前模型: " << model.getModelName() << ", 模型面数: " << model.getFaceNum() << std::endl;
    BasicZBuffer basicZBuffer(scene.getWidth(), scene.getHeight());
    basicZBuffer.rasterizeScene(model, scene);
    basicZBuffer.render();
    basicZBuffer.showInfo();
    model.clear();
    ScanlineZBuffer scanlineZBuffer(scene.getWidth(), scene.getHeight());
    scanlineZBuffer.rasterizeScene(model, scene);
    scanlineZBuffer.render();
    scanlineZBuffer.showInfo();
    model.clear();
    scanlineZBuffer.setMode(1);
    scanlineZBuffer.rasterizeScene(model, scene);
    scanlineZBuffer.render();
    scanlineZBuffer.showInfo();
    model.clear();
    BasicHierarchicalZBuffer basicHierarchicalZBuffer(scene.getWidth(), scene.getHeight());
    basicHierarchicalZBuffer.rasterizeScene(model, scene);
    basicHierarchicalZBuffer.render();
    basicHierarchicalZBuffer.showInfo();
    model.clear();
    HierarchicalZBuffer hierarchicalZBuffer(scene.getWidth(), scene.getHeight());
    hierarchicalZBuffer.rasterizeScene(model, scene);
    hierarchicalZBuffer.render();
    hierarchicalZBuffer.showInfo();
    model.clear();
    //完整模式的另一种场景加速结构：八叉树
    HierarchicalZBuffer octreeZBuffer(scene.getWidth(), scene.getHeight());
    octreeZBuffer.setSceneStructure(HierarchicalZBuffer::SceneStructure::Octree);
    octreeZBuffer.rasterizeScene(model, scene);
    octreeZBuffer.render();
    octreeZBuffer.showInfo();
}

/*! @brief 自带模型用的默认场景（相机在模型外，单向光）
 */
static Scene defaultScene() {
    return Scene();
}

/*! @brief 建筑类场景（sponza）的机位与光照
 *
 *  模型会被归一化到 [-1,1]^3（按最长轴缩放并居中），所以整栋 sponza 实际只有
 *  2.0 x 0.84 x 1.23 那么大 —— 默认相机在 z = 1.8 处，是整个建筑的外面，只能看到一堵外墙。
 *  这里把相机放进门厅里，同时把视锥按室内尺度收小：默认的 fov 90、近平面距离 0.3
 *  在室内会让最近的墙壁直接糊满整个画面。
 *  另外单向光对闭合的凸模型够用，对建筑内部不行 —— 背光的那一半面会渲染成纯黑，
 *  所以给一点环境光。
 */
static Scene::Config sponzaConfig() {
    Scene::Config config;
    config.camera = glm::vec3(0.1f, 0.0f, 0.0f);
    config.eyeDirection = glm::vec3(-1.0f, 0.0f, 0.0f);
    config.lightDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    config.nearPlane = -0.02f;
    config.farPlane = -30.0f;
    config.fov = 70.0f;
    config.ambient = 0.55f;
    return config;
}

int main(int argc, char** argv) {
    bool withSponza = true;
    //for (int i = 1; i < argc; i++) {
    //    if (std::string(argv[i]) == "--sponza") {
    //        withSponza = true;
    //    }
    //}
    Scene scene = defaultScene();
    const int modelNum = 6;
    std::string modelPaths[] = { "models/dolphins.obj", "models/african_head.obj", "models/teapot.obj",
        "models/bunny.obj", "models/robot.obj", "models/armadillo.obj" };
    Model* models = new Model[modelNum];
    for (int i = 0; i < modelNum; i++) {
        models[i].loadModel(modelPaths[i]);
        renderAll(models[i], scene);
    }
    delete[] models;
    if (withSponza) {
        //sponza 是建筑内部场景，机位、视锥与光照都与自带模型不同，见 sponzaConfig 的说明
        Scene interiorScene(sponzaConfig());
        Model sponza;
        sponza.loadModel("models/sponza.obj");
        renderAll(sponza, interiorScene);
    }
    return 0;
}
