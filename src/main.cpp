#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "model.hpp"
#include "scene.hpp"
#include "occlusionScene.hpp"
#include "outputPath.hpp"
#include "commandLine.hpp"
#include "basicZBuffer.hpp"
#include "scanlineZBuffer.hpp"
#include "basicHierarchicalZBuffer.hpp"
#include "hierarchicalZBuffer.hpp"

/*! @brief 六种算法的名字
 *
 *  这些名字同时也是结果图文件名的前缀（<算法名>_<场景名>.png），
 *  所以 --alg 与 --verify 都用同一套名字，不需要额外的映射表。
 */
static const std::vector<std::string> kAlgorithmNames = {
    "BasicZBuffer", "ClassicScanlineZBuffer", "SpecialScanlineZBuffer",
    "BasicHierarchicalZBuffer", "HierarchicalZBuffer", "OctreeHierarchicalZBuffer"
};

/*! @brief 一个待渲染的场景
 *
 *  objPath 非空 -> 从 obj 文件加载；为空 -> 用 occlusionScene.hpp 在程序内生成。
 */
struct SceneEntry {
    const char* name;
    const char* objPath;
    bool interior;      //是否用建筑内部机位（sponzaConfig）
    int gridSize;       //以下三项只在程序内生成时使用
    bool nearToFar;
    float cellRatio;
};

static const SceneEntry kScenes[] = {
    { "dolphins",                 "models/dolphins.obj",     false, 0,  true,  1.0f },
    { "african_head",             "models/african_head.obj", false, 0,  true,  1.0f },
    { "teapot",                   "models/teapot.obj",       false, 0,  true,  1.0f },
    { "bunny",                    "models/bunny.obj",        false, 0,  true,  1.0f },
    { "robot",                    "models/robot.obj",        false, 0,  true,  1.0f },
    { "armadillo",                "models/armadillo.obj",    false, 0,  true,  1.0f },
    { "sponza",                   "models/sponza.obj",       true,  0,  true,  1.0f },
    { "occlusion_M20_n2f",        nullptr,                   false, 20, true,  1.06f },
    { "occlusion_M20_f2n",        nullptr,                   false, 20, false, 1.06f },
    { "occlusion_M28_n2f",        nullptr,                   false, 28, true,  1.06f },
    { "occlusion_M28_gapped_n2f", nullptr,                   false, 28, true,  0.90f },
};
constexpr int kSceneCount = sizeof(kScenes) / sizeof(kScenes[0]);

/*! @brief 建筑类场景（sponza）的机位与光照
 *
 *  模型会被归一化到 [-1,1]^3（按最长轴缩放并居中），所以整栋 sponza 实际只有
 *  2.0 x 0.84 x 1.23 那么大 —— 默认相机在 z = 1.8 处，是整个建筑的外面，只能看到一堵外墙。
 *  这里把相机放进门厅里、沿长廊朝 -x 看，同时把视锥按室内尺度收小：默认的 fov 90、
 *  近平面距离 0.3 在室内会让最近的墙壁直接糊满整个画面。
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

/*! @brief 算法名 -> 打印用的显示名（与 showInfo 的输出保持一致）
 */
static std::string displayName(const std::string& algorithm) {
    if (algorithm == "ClassicScanlineZBuffer") return "经典ScanlineZBuffer";
    if (algorithm == "SpecialScanlineZBuffer") return "特化ScanlineZBuffer";
    return algorithm;
}

/*! @brief 在给定列表里查找名字
 *  @return 找到返回 true；list 为空表示"全部"，也返回 true
 */
static bool isSelected(const std::vector<std::string>& list, const std::string& name) {
    return list.empty() || std::find(list.begin(), list.end(), name) != list.end();
}

/*! @brief 检查列表里的名字是否都合法，有非法名字时报错
 */
static bool checkNames(const std::vector<std::string>& list, const std::vector<std::string>& valid,
                       const char* what) {
    for (const std::string& name : list) {
        if (std::find(valid.begin(), valid.end(), name) == valid.end()) {
            std::cerr << "参数错误: 未知的" << what << " \"" << name << "\"；可用值见 --list。" << std::endl;
            return false;
        }
    }
    return true;
}

/*! @brief 列出可用的场景与算法
 */
static void printAvailable() {
    std::cout << "可用场景（--scene 用这些名字）:" << std::endl;
    for (int i = 0; i < kSceneCount; i++) {
        const std::string name = kScenes[i].name;
        std::cout << "  " << name << std::string(name.size() < 26 ? 26 - name.size() : 1, ' ');
        if (kScenes[i].objPath != nullptr) {
            std::cout << (kScenes[i].interior ? "OBJ 模型（建筑内部机位）" : "OBJ 模型");
        } else {
            std::cout << "程序内生成（" << kScenes[i].gridSize << "^3 体素，";
            std::cout << (kScenes[i].nearToFar ? "近→远" : "远→近") << "，重叠系数 " << kScenes[i].cellRatio << "）";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl << "可用算法（--alg 用这些名字）:" << std::endl;
    for (const std::string& name : kAlgorithmNames) {
        std::cout << "  " << name << std::endl;
    }
    std::cout << std::endl << "六种算法分别对应结果图文件名里 <算法名>_<场景名>.png 的前缀。" << std::endl;
}

/*! @brief 统计一组数
 *  @param[in] values: 采样值（会被排序）
 *  @param[in] statistics: min / mean / median
 *  @return 统计结果
 */
static double statisticOf(std::vector<double> values, const std::string& statistics) {
    std::sort(values.begin(), values.end());
    if (statistics == "min") return values.front();
    if (statistics == "median") return values[values.size() / 2];
    double sum = 0.0;
    for (double value : values) sum += value;
    return sum / static_cast<double>(values.size());
}

/*! @brief 跑一个算法一遍
 *
 *  每次都构造一个新的算法对象：层次结构（金字塔 / BVH / 八叉树）每帧重建的开销
 *  本来就计入"总时间"，这样重复多次时的口径才和单次运行一致。
 *
 *  @param[in] algorithm: 算法名
 *  @param[in,out] model: 模型
 *  @param[in] scene: 场景
 *  @param[in] writeImage: 这一遍是否写出结果图
 *  @param[in] printDetail: 是否调用 showInfo 打印明细（单次运行时才需要）
 *  @param[out] renderTimes: 追加渲染耗时
 *  @param[out] totalTimes: 追加总耗时
 */
static void runOnce(const std::string& algorithm, Model& model, Scene& scene, bool writeImage,
                    bool printDetail, std::vector<double>& renderTimes, std::vector<double>& totalTimes) {
    const int width = scene.getWidth(), height = scene.getHeight();
    if (algorithm == "BasicZBuffer") {
        BasicZBuffer object(width, height);
        object.rasterizeScene(model, scene);
        if (writeImage) object.render();
        if (printDetail) object.showInfo();
        renderTimes.push_back(object.getRenderTime());
        totalTimes.push_back(object.getTotalTime());
    } else if (algorithm == "ClassicScanlineZBuffer" || algorithm == "SpecialScanlineZBuffer") {
        ScanlineZBuffer object(width, height);
        object.setMode(algorithm == "SpecialScanlineZBuffer" ? 1 : 0);
        object.rasterizeScene(model, scene);
        if (writeImage) object.render();
        if (printDetail) object.showInfo();
        renderTimes.push_back(object.getRenderTime());
        totalTimes.push_back(object.getTotalTime());
    } else if (algorithm == "BasicHierarchicalZBuffer") {
        BasicHierarchicalZBuffer object(width, height);
        object.rasterizeScene(model, scene);
        if (writeImage) object.render();
        if (printDetail) object.showInfo();
        renderTimes.push_back(object.getRenderTime());
        totalTimes.push_back(object.getTotalTime());
    } else {
        HierarchicalZBuffer object(width, height);
        object.setSceneStructure(algorithm == "OctreeHierarchicalZBuffer"
            ? HierarchicalZBuffer::SceneStructure::Octree
            : HierarchicalZBuffer::SceneStructure::BVH);
        object.rasterizeScene(model, scene);
        if (writeImage) object.render();
        if (printDetail) object.showInfo();
        renderTimes.push_back(object.getRenderTime());
        totalTimes.push_back(object.getTotalTime());
    }
}

/*! @brief 用选中的算法渲染一个场景
 *  @param[in,out] model: 模型
 *  @param[in] scene: 场景
 *  @param[in] options: 命令行选项
 *  @param[out] producedFiles: 本次产出的结果图文件名（用于 --verify）
 */
static void renderScene(Model& model, Scene& scene, const CommandLineOptions& options,
                        std::vector<std::string>& producedFiles) {
    std::cout << "当前模型: " << model.getModelName() << ", 模型面数: " << model.getFaceNum() << std::endl;
    const bool printDetail = (options.repeat == 1);
    for (const std::string& algorithm : kAlgorithmNames) {
        if (!isSelected(options.algorithms, algorithm)) {
            continue;
        }
        std::vector<double> renderTimes, totalTimes;
        for (int iteration = 0; iteration < options.repeat; iteration++) {
            //只在最后一遍写图：重复多次时写 N 张一模一样的图没有意义
            const bool writeImage = options.writeImages && (iteration + 1 == options.repeat);
            runOnce(algorithm, model, scene, writeImage, printDetail, renderTimes, totalTimes);
            model.clear();
        }
        if (options.writeImages) {
            producedFiles.push_back(algorithm + "_" + model.getModelName() + ".png");
        }
        if (!printDetail) {
            const double render = statisticOf(renderTimes, options.statistics);
            const double total = statisticOf(totalTimes, options.statistics);
            const double minimum = *std::min_element(totalTimes.begin(), totalTimes.end());
            const double maximum = *std::max_element(totalTimes.begin(), totalTimes.end());
            std::cout << displayName(algorithm) << ": 渲染时间为" << render << "ms, "
                      << "建结构时间为" << (total - render) << "ms, 总时间为" << total << "ms"
                      << "   (" << options.repeat << " 次 " << options.statistics
                      << "，总时间 min " << minimum << " / max " << maximum << "ms)" << std::endl;
        }
    }
}

/*! @brief 逐字节比较两个文件
 *  @return true 表示两个文件都存在且内容完全相同
 */
static bool compareFiles(const std::string& expected, const std::string& actual) {
    std::ifstream expectedStream(expected, std::ios::binary), actualStream(actual, std::ios::binary);
    if (!expectedStream || !actualStream) {
        return false;
    }
    const std::string expectedData((std::istreambuf_iterator<char>(expectedStream)), std::istreambuf_iterator<char>());
    const std::string actualData((std::istreambuf_iterator<char>(actualStream)), std::istreambuf_iterator<char>());
    return !expectedData.empty() && expectedData == actualData;
}

/*! @brief 把本次产出的结果图与金标准目录逐字节比对
 *  @param[in] files: 本次产出的文件名
 *  @param[in] referenceDirectory: 金标准目录
 *  @return 不同的张数
 */
static int verifyAgainst(const std::vector<std::string>& files, const std::string& referenceDirectory) {
    std::cout << "与金标准比对: " << referenceDirectory << "（" << files.size() << " 张）" << std::endl;
    int identical = 0, different = 0;
    for (const std::string& name : files) {
        const std::string produced = hzb::outputDirectory() + "/" + name;
        const std::string expected = referenceDirectory + "/" + name;
        if (compareFiles(produced, expected)) {
            identical++;
        } else {
            different++;
            std::ifstream probe(expected, std::ios::binary);
            std::cout << (probe ? "  不同: " : "  金标准缺失: ") << name << std::endl;
        }
    }
    std::cout << "IDENTICAL=" << identical << "  DIFFERENT=" << different << std::endl;
    return different;
}

int main(int argc, char** argv) {
    CommandLineOptions options;
    if (!parseCommandLine(argc, argv, options)) {
        return 2;
    }
    if (options.help) {
        printUsage(argv[0]);
        return 0;
    }
    if (options.list) {
        printAvailable();
        return 0;
    }
    std::vector<std::string> sceneNames;
    for (int i = 0; i < kSceneCount; i++) {
        sceneNames.push_back(kScenes[i].name);
    }
    if (!checkNames(options.scenes, sceneNames, "场景") ||
        !checkNames(options.algorithms, kAlgorithmNames, "算法")) {
        return 2;
    }
    if (!options.writeImages && !options.verifyDirectory.empty()) {
        std::cerr << "参数错误: --verify 要比对结果图，不能和 --no-images 一起用。" << std::endl;
        return 2;
    }

    std::vector<std::string> producedFiles;
    int renderedScenes = 0;
    for (int i = 0; i < kSceneCount; i++) {
        const SceneEntry& entry = kScenes[i];
        if (!isSelected(options.scenes, entry.name)) {
            continue;
        }
        Model model;
        if (entry.objPath != nullptr) {
            model.loadModel(entry.objPath);
            if (model.getFaceNum() == 0) {
                std::cerr << "模型加载失败: " << entry.objPath << "（工作目录需要是工程根目录）" << std::endl;
                return 2;
            }
        } else {
            buildOcclusionScene(model, entry.gridSize, entry.nearToFar, entry.cellRatio, entry.name);
        }
        Scene scene = entry.interior ? Scene(sponzaConfig()) : Scene();
        renderScene(model, scene, options, producedFiles);
        renderedScenes++;
    }

    if (renderedScenes == 0) {
        std::cout << "没有场景被选中。" << std::endl;
        return 0;
    }
    if (!options.verifyDirectory.empty()) {
        return verifyAgainst(producedFiles, options.verifyDirectory) == 0 ? 0 : 1;
    }
    return 0;
}
