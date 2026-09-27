#ifndef __COMMAND_LINE_HPP__
#define __COMMAND_LINE_HPP__

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "outputPath.hpp"

/*! @brief 命令行选项
 *
 *  全部字段都有"不指定时等同于原来行为"的默认值，
 *  所以不带任何参数运行与加参数之前完全一致。
 */
struct CommandLineOptions {
	/*! @brief 要渲染的场景名；空表示全部 */
	std::vector<std::string> scenes;
	/*! @brief 要运行的算法名；空表示全部 */
	std::vector<std::string> algorithms;
	/*! @brief 每个场景每个算法重复渲染的遍数 */
	int repeat = 1;
	/*! @brief 重复多次时报告的统计量：min / mean / median */
	std::string statistics = "mean";
	/*! @brief 是否写出结果图 */
	bool writeImages = true;
	/*! @brief 非空时：渲染后与这个目录下的图逐字节比对 */
	std::string verifyDirectory;
	/*! @brief 只列出可用的场景与算法 */
	bool list = false;
	/*! @brief 显示用法 */
	bool help = false;
	/*! @brief 解析是否出错 */
	bool parseFailed = false;
};

/*! @brief 按逗号拆分字符串，顺便去掉空格与空项
 *  @param[in] text: 待拆分的字符串
 *  @return 拆分结果
 */
inline std::vector<std::string> splitByComma(const std::string& text) {
	std::vector<std::string> parts;
	std::string current;
	for (char c : text) {
		if (c == ',') {
			if (!current.empty()) parts.push_back(current);
			current.clear();
		} else if (c != ' ') {
			current += c;
		}
	}
	if (!current.empty()) parts.push_back(current);
	return parts;
}

/*! @brief 打印用法
 *  @param[in] programName: 可执行文件名
 */
inline void printUsage(const char* programName) {
	std::cout <<
		"用法: " << programName << " [选项]\n"
		"\n"
		"不带任何选项时渲染全部场景 x 全部算法，结果写进 results/。\n"
		"\n"
		"场景与算法选择:\n"
		"  -s, --scene <名>[,名...]   只渲染指定场景（--list 看可用名字）\n"
		"  -a, --alg   <名>[,名...]   只运行指定算法\n"
		"  -l, --list                 列出可用的场景与算法后退出\n"
		"\n"
		"运行控制:\n"
		"  -n, --repeat <次数>        每个场景每个算法重复渲染的遍数（默认 1）\n"
		"      --stats  <min|mean|median>\n"
		"                             重复多次时报告的统计量（默认 mean）\n"
		"  -o, --output <目录>        结果图输出目录（默认 results）\n"
		"      --no-images            不写结果图，只计时\n"
		"\n"
		"回归验证:\n"
		"      --verify <目录>        渲染后与 <目录> 下的图逐字节比对，\n"
		"                             有任何一张不同就返回非零退出码\n"
		"\n"
		"  -h, --help                 显示本帮助\n"
		"\n"
		"示例:\n"
		"  " << programName << " --scene sponza --alg HierarchicalZBuffer,OctreeHierarchicalZBuffer\n"
		"  " << programName << " --repeat 10 --no-images            # 跑 10 遍取平均\n"
		"  " << programName << " --output out --verify golden       # 与金标准比对\n";
}

/*! @brief 解析命令行
 *
 *  遇到无法识别的选项、缺少参数或参数非法时，打印原因、置 parseFailed 并返回 false。
 *
 *  @param[in] argc: 参数个数
 *  @param[in] argv: 参数数组
 *  @param[out] options: 解析结果
 *  @return 是否解析成功
 */
inline bool parseCommandLine(int argc, char** argv, CommandLineOptions& options) {
	auto fail = [&](const std::string& reason) {
		std::cerr << "参数错误: " << reason << "\n";
		std::cerr << "用 --help 查看用法。" << std::endl;
		options.parseFailed = true;
	};
	for (int i = 1; i < argc; i++) {
		const std::string argument = argv[i];
		auto takeValue = [&](const char* what) -> std::string {
			if (i + 1 >= argc) {
				fail(std::string("选项 ") + argument + " 缺少" + what);
				return std::string();
			}
			return argv[++i];
		};
		if (argument == "-h" || argument == "--help") {
			options.help = true;
		} else if (argument == "-l" || argument == "--list") {
			options.list = true;
		} else if (argument == "-s" || argument == "--scene") {
			options.scenes = splitByComma(takeValue("场景名"));
		} else if (argument == "-a" || argument == "--alg") {
			options.algorithms = splitByComma(takeValue("算法名"));
		} else if (argument == "-o" || argument == "--output") {
			const std::string value = takeValue("目录");
			if (!value.empty()) hzb::outputDirectory() = value;
		} else if (argument == "-n" || argument == "--repeat") {
			const std::string value = takeValue("次数");
			const int count = std::atoi(value.c_str());
			if (count < 1) {
				fail("--repeat 需要一个正整数，收到的是 \"" + value + "\"");
			} else {
				options.repeat = count;
			}
		} else if (argument == "--stats") {
			const std::string value = takeValue("统计量");
			if (value != "min" && value != "mean" && value != "median") {
				fail("--stats 只支持 min / mean / median，收到的是 \"" + value + "\"");
			} else {
				options.statistics = value;
			}
		} else if (argument == "--no-images") {
			options.writeImages = false;
		} else if (argument == "--verify") {
			options.verifyDirectory = takeValue("目录");
		} else {
			fail("无法识别的选项 \"" + argument + "\"");
		}
	}
	return !options.parseFailed;
}

#endif // !__COMMAND_LINE_HPP__
