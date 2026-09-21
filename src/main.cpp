#include <iostream>
#include "model.hpp"
#include "scene.hpp"
#include "basicZBuffer.hpp"
#include "scanlineZBuffer.hpp"
#include "basicHierarchicalZBuffer.hpp"
#include "hierarchicalZBuffer.hpp"

int main() {
    Scene scene;
    int modelNum = 6;
    std::string modelPaths[] = { "models/dolphins.obj", "models/african_head.obj", "models/teapot.obj",
    	"models/bunny.obj", "models/robot.obj", "models/armadillo.obj"};
    Model* models = new Model[modelNum];
    for (int i = 0; i < modelNum; i++) {
        models[i].loadModel(modelPaths[i]);
        std::cout << "当前模型: " << models[i].getModelName() << ", 模型面数: " << models[i].getFaceNum() << std::endl;
        BasicZBuffer basicZBuffer(scene.getWidth(), scene.getHeight());
        basicZBuffer.rasterizeScene(models[i], scene);
        basicZBuffer.render();
        basicZBuffer.showInfo();
        models[i].clear();
        ScanlineZBuffer scanlineZBuffer(scene.getWidth(), scene.getHeight());
        scanlineZBuffer.rasterizeScene(models[i], scene);
        scanlineZBuffer.render();
        scanlineZBuffer.showInfo();
        models[i].clear();
        scanlineZBuffer.setMode(1);
        scanlineZBuffer.rasterizeScene(models[i], scene);
        scanlineZBuffer.render();
        scanlineZBuffer.showInfo();
        models[i].clear();
        BasicHierarchicalZBuffer basicHierarchicalZBuffer(scene.getWidth(), scene.getHeight());
        basicHierarchicalZBuffer.rasterizeScene(models[i], scene);
        basicHierarchicalZBuffer.render();
        basicHierarchicalZBuffer.showInfo();
        models[i].clear();
        HierarchicalZBuffer hierarchicalZBuffer(scene.getWidth(), scene.getHeight());
        hierarchicalZBuffer.rasterizeScene(models[i], scene);
        hierarchicalZBuffer.render();
        hierarchicalZBuffer.showInfo();
    }
    return 0;
}
