#ifndef __OUTPUT_PATH_HPP__
#define __OUTPUT_PATH_HPP__

#include <string>

/*! @brief 结果图的输出路径
 *
 *  各算法的 render() 都从这里拼路径，而不是把 "results/" 写死 ——
 *  这样命令行可以用 --output 指定别的目录，测试脚本与正式结果就不会互相覆盖。
 */
namespace hzb {

/*! @brief 结果图输出目录，默认 "results"；启动时由 main 按命令行参数设置
 *  @return 输出目录
 */
inline std::string& outputDirectory() {
	static std::string directory = "results";
	return directory;
}

/*! @brief 拼出结果图的完整路径：<输出目录>/<算法名>_<模型名>.png
 *  @param[in] algorithm: 算法名（与结果图文件名前缀一致）
 *  @param[in] model: 模型名
 *  @return 结果图的完整路径
 */
inline std::string outputPath(const std::string& algorithm, const std::string& model) {
	return outputDirectory() + "/" + algorithm + "_" + model + ".png";
}

}   // namespace hzb

#endif // !__OUTPUT_PATH_HPP__
