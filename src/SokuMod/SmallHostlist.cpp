//
// Created by PinkySmile on 07/01/2023.
//

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <climits>
#include <set>
#include <mlang.h>
#include <objbase.h>

#include "nlohmann/json.hpp"
#include "data.hpp"
#include "SmallHostlist.hpp"
#include "LobbyData.hpp"
#include "createUTFTexture.hpp"
#include "encodingConverter.hpp"

#define MAX_OVERLAY_ANIMATION 15
#define MAX_VISIBLE_ENTRIES 11
#define modifyPos(x, y) ((SokuLib::Vector2i{static_cast<int>(x), static_cast<int>(y)} * this->_ratio + this->_pos).to<int>())

static bool decodeCodePage(const std::string &input, unsigned codePage, DWORD flags, std::wstring &output)
{
	if (input.empty()) {
		output.clear();
		return true;
	}
	auto size = MultiByteToWideChar(codePage, flags, input.data(), static_cast<int>(input.size()), nullptr, 0);
	if (!size)
		return false;
	output.resize(size);
	if (!MultiByteToWideChar(codePage, flags, input.data(), static_cast<int>(input.size()), output.data(), size)) {
		output.clear();
		return false;
	}
	return true;
}

static bool encodeCodePageExact(const std::wstring &input, unsigned codePage, std::string &output)
{
	if (input.empty()) {
		output.clear();
		return true;
	}
	bool unicodeCodePage = codePage == CP_UTF8 || codePage == 54936;
	auto flags = unicodeCodePage ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
	BOOL usedDefault = FALSE;
	auto usedDefaultPtr = unicodeCodePage ? nullptr : &usedDefault;
	auto size = WideCharToMultiByte(codePage, flags, input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, usedDefaultPtr);
	if (!size || usedDefault)
		return false;
	output.resize(size);
	usedDefault = FALSE;
	if (!WideCharToMultiByte(codePage, flags, input.data(), static_cast<int>(input.size()), output.data(), size, nullptr, usedDefaultPtr) || usedDefault) {
		output.clear();
		return false;
	}
	return true;
}

static std::map<unsigned, int> detectCodePages(const std::string &input)
{
	HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	CLSID classId;
	IID interfaceId;
	IMultiLanguage2 *multiLanguage = nullptr;
	std::map<unsigned, int> result;

	if (
		FAILED(CLSIDFromString(L"{275C23E2-3747-11D0-9FEA-00AA003F8646}", &classId)) ||
		FAILED(IIDFromString(L"{DCCFC164-2B38-11D2-B7EC-00C04F8F5D9A}", &interfaceId)) ||
		FAILED(CoCreateInstance(classId, nullptr, CLSCTX_INPROC_SERVER, interfaceId, reinterpret_cast<void **>(&multiLanguage)))
	) {
		if (SUCCEEDED(initResult))
			CoUninitialize();
		return result;
	}

	DetectEncodingInfo detections[16]{};
	INT sourceSize = static_cast<INT>(input.size());
	INT detectionCount = sizeof(detections) / sizeof(*detections);
	if (SUCCEEDED(multiLanguage->DetectInputCodepage(MLDETECTCP_NONE, 0, const_cast<char *>(input.data()), &sourceSize, detections, &detectionCount))) {
		for (INT i = 0; i < detectionCount; i++) {
			if (detections[i].nCodePage != 932 && detections[i].nCodePage != 936 && detections[i].nCodePage != 950 && detections[i].nCodePage != 54936)
				continue;
			result[detections[i].nCodePage] = detections[i].nConfidence;
		}
	}
	multiLanguage->Release();
	if (SUCCEEDED(initResult))
		CoUninitialize();
	return result;
}

struct DecodedNameCandidate {
	std::wstring text;
	unsigned codePage;
	int detectorConfidence;
	bool directUtf8;
	bool recoveredFromShiftJis;
};

static int scoreDecodedName(const DecodedNameCandidate &candidate, const std::string &country)
{
	static constexpr const wchar_t *commonIdeographs =
		L"\u7684\u4e00\u662f\u4e0d\u4e86\u4eba\u6211\u5728\u6709\u4ed6\u8fd9\u4e3a\u4e4b\u5927\u6765\u4ee5\u4e2a\u4e2d\u4e0a"
		L"\u4eec\u5230\u8bf4\u56fd\u548c\u5730\u4e5f\u5b50\u65f6\u9053\u51fa\u800c\u8981\u4e8e\u5c31\u4e0b\u5f97\u53ef\u4f60\u5e74\u751f\u81ea"
		L"\u4f1a\u90a3\u540e\u80fd\u5bf9\u7740\u4e8b\u5176\u91cc\u6240\u53bb\u884c\u8fc7\u5bb6\u5341\u7528\u53d1\u5929\u5982\u7136\u4f5c\u65b9"
		L"\u6210\u8005\u591a\u65e5\u90fd\u4e09\u5c0f\u4e8c\u65e0\u540c\u7ecf\u6cd5\u5f53\u8d77\u4e0e\u597d\u770b\u5b66\u8fdb\u79cd\u5c06\u8fd8\u5206"
		L"\u5fc3\u524d\u9762\u53c8\u5b9a\u89c1\u53ea\u4e3b\u6ca1\u516c\u4ece\u5df2\u77e5\u5168\u73b0\u60c5\u660e\u60f3\u5916\u95f4\u6837\u672c\u9c7c"
		L"\u732b\u7ed8\u7b14\u6c5f\u5357\u73e0\u6d32\u5c9b\u6816\u4fe1\u98d8\u51f9\u627f\u8bfa\u54b8\u4ed3\u9f20\u795e\u7ea2\u5e08\u5085\u5f00\u9501";
	static constexpr const wchar_t *simplifiedOnly =
		L"\u8fd9\u4eec\u56fd\u53d1\u540e\u91cc\u5bf9\u65f6\u4e3a\u4e0e\u4e1c\u4e1d\u4e50\u4e60\u4e61\u4e66\u4e70\u4e91\u4e9a\u4ea7\u4eb2\u4ebf\u4ec5\u4ece\u4ed3\u4eea\u4eec\u4f1a\u4f1e\u4f1f\u4f20\u4f24\u4f26\u4f53\u4f59\u4fa0\u4fa3\u4fa5\u4fa6\u4fa7\u4fa8\u4fa9\u4fe9\u5019\u503a\u503e\u507f\u50a8\u513f\u5151\u5170\u5173\u5174\u517b\u517d\u5185\u5188\u5199\u519b\u519c\u51b2\u51b3\u51bb\u51c0\u51c6\u51e0\u51fb\u5218\u5219\u521a\u521b\u5220\u522b\u5239\u5236\u5237\u5238\u5239\u5242\u5251\u5267\u529e\u52a1\u52a8\u52b1\u52b2\u52b3\u52bf\u52cb\u5326\u533a\u534e\u5355\u5356\u5362\u536b\u5382\u5385\u5386\u5389\u538b\u538c\u53a2\u53bf\u53c2\u53cc\u53d8\u53e0\u53f6\u53f7\u53f9\u5417\u542f\u5434\u5458\u545b\u548f\u54cd\u54d1\u54d7\u5524\u556c\u55b7\u56e2\u56ed\u56f4\u56fe\u5706\u573a\u574f\u5757\u575a\u575b\u575d\u575e\u57ab\u57f9\u57fa\u5815\u58f0\u5904\u5907\u591f\u5934\u5938\u5939\u593a\u594b\u5956\u5987\u5988\u5a07\u5b66\u5b9d\u5b9e\u5ba0\u5ba1\u5baa\u5bab\u5bbd\u5bbe\u5bf9\u5bfb\u5bfc\u5c06\u5c14\u5c18\u5c1d\u5c42\u5c81\u5c82\u5c97\u5c9a\u5c9b\u5cad\u5e01\u5e05\u5e08\u5e10\u5e18\u5e1c\u5e26\u5e2e\u5e72\u5e7f\u5e84\u5e86\u5e93\u5e94\u5e99\u5e9e\u5e9f\u5f00\u5f02\u5f20\u5f3a\u5f52\u5f55\u5f84\u5fc6\u5fe7\u6000\u6001\u601c\u603b\u604b\u6052\u6076\u607c\u60a6\u60ac\u60ca\u60e8\u60ef\u6108\u611a\u613f\u620f\u6218\u6237\u6251\u6267\u6269\u626b\u626c\u6270\u629a\u62a2\u62a4\u62a5\u62c5\u62df\u62e5\u62e6\u62e9\u6302\u6311\u6316\u631a\u6323\u6325\u632a\u635f\u6362\u636e\u6389\u6392\u63a5\u63a7\u63b7\u63ba\u63fd\u6446\u6447\u644a\u6491\u64a4\u64cd\u64ce\u64de\u6536\u654c\u6559\u6570\u65ad\u65e0\u65e7\u65f6\u663e\u6653\u6682\u672f\u673a\u6740\u6742\u6743\u6761\u6765\u6768\u6770\u6781\u6784\u679c\u67a3\u67aa\u67ab\u67dc\u6807\u6808\u6811\u6837\u6865\u6866\u68a6\u68c0\u692d\u697c\u6b22\u6b27\u6b7b\u6b8b\u6bb4\u6bc1\u6bd5\u6c14\u6c49\u6c64\u6c9f\u6ca1\u6ca7\u6ca9\u6caa\u6cde\u6cfb\u6d01\u6d45\u6d46\u6d4b\u6d4e\u6d51\u6d53\u6d77\u6d9b\u6da1\u6da3\u6da8\u6e05\u6e10\u6e29\u6e7e\u6e7f\u6e83\u6eda\u6ee1\u6ee4\u6ee5\u6f47\u6f5c\u706d\u706f\u7075\u707e\u70bc\u70bd\u70e6\u70ed\u7231\u7237\u7275\u72b6\u72ec\u72ed\u72f1\u730e\u732a\u732b\u73b0\u73af\u7391\u73ba\u74ef\u7535\u753b\u7545\u7597\u75af\u76d1\u76d8\u76d7\u76cf\u76d6\u7750\u77e9\u77ff\u7801\u7816\u781a\u7834\u7855\u786e\u788d\u793c\u795e\u7985\u79bb\u79cd\u79ef\u79f0\u7a0e\u7a33\u7a77\u7a83\u7a8d\u7ade\u7b14\u7b3c\u7b49\u7b80\u7c7b\u7cae\u7d27\u7ea0\u7ea2\u7ea6\u7ea7\u7eaa\u7eb3\u7eb5\u7eb7\u7eb8\u7eb9\u7eba\u7ebd\u7ebf\u7ec4\u7ec6\u7ec7\u7ec8\u7eca\u7ecf\u7ed1\u7ed3\u7ed5\u7ed8\u7ed9\u7eda\u7edc\u7edd\u7edf\u7ee7\u7eed\u7ef4\u7efc\u7eff\u7f16\u7f18\u7f1a\u7f1d\u7f20\u7f29\u7f34\u7f50\u7f51\u7f57\u7f5a\u7f62\u7f69\u7f6a\u7f6e\u7f72\u7f8e\u7fa4\u7fd4\u8001\u8010\u804c\u8054\u80a4\u80bf\u80c0\u80c1\u80c6\u80dc\u80f6\u8109\u810f\u8111\u811a\u8131\u8138\u817e\u822c\u8230\u827a\u8282\u82cf\u8303\u830e\u8350\u836f\u83b7\u83b2\u83dc\u83f2\u8425\u843d\u84dd\u84dd\u85cf\u865a\u866b\u867d\u867e\u8680\u8865\u8868\u8884\u88c5\u88c5\u897f\u89c1\u89c2\u89c4\u89c9\u89e6\u8ba1\u8ba2\u8ba4\u8ba8\u8ba9\u8bad\u8bae\u8bb0\u8bb2\u8bb8\u8bba\u8bbe\u8bbf\u8bc1\u8bc4\u8bc6\u8bc9\u8bca\u8bcd\u8bd1\u8bd5\u8bd7\u8bda\u8bdd\u8be2\u8be5\u8be6\u8bed\u8bef\u8bf4\u8bf7\u8bf8\u8bfb\u8bfe\u8c01\u8c03\u8c08\u8c0b\u8c0e\u8c22\u8c31\u8c37\u8d1d\u8d1f\u8d22\u8d23\u8d25\u8d27\u8d28\u8d2d\u8d39\u8d44\u8d4b\u8d4c\u8d56\u8d5a\u8d5e\u8d60\u8d75\u8d76\u8d8b\u8dc3\u8f66\u8f68\u8f6c\u8f6e\u8f6f\u8f7b\u8f7d\u8f83\u8f85\u8f86\u8f88\u8f89\u8f91\u8f93\u8f9e\u8fb9\u8fbe\u8fc1\u8fc7\u8fd0\u8fd8\u8fdb\u8fde\u8fdf\u9002\u9009\u9012\u9057\u9065\u90ae\u90bb\u90d1\u915d\u91ca\u91cc\u9274\u94a2\u94a5\u94b1\u94bb\u94c1\u94c3\u94dc\u94f6\u94fa\u94fe\u9500\u9501\u9519\u9526\u952e\u9547\u955c\u957f\u95e8\u95ea\u95ed\u95ee\u95ef\u95f2\u95f4\u95f7\u95f9\u95fb\u961f\u9633\u9634\u9635\u9636\u9645\u9646\u9648\u964c\u964d\u9650\u9669\u968f\u9690\u96be\u96fe\u9759\u97e9\u9875\u9876\u9879\u987a\u987b\u987e\u9884\u9886\u9886\u9891\u9898\u989c\u98ce\u98de\u996d\u996e\u9970\u9971\u997c\u9986\u9996\u9a6c\u9a71\u9a8c\u9a91\u9a97\u9a9a\u9aa8\u9ad8\u9b3c\u9c7c\u9e1f\u9e21\u9e23\u9e3f\u9e45\u9ec4\u9ed1\u9f50\u9f7f\u9f99";
	static constexpr const wchar_t *traditionalOnly = L"\u9019\u5011\u570b\u767c\u5f8c\u88e1\u5c0d\u6642\u70ba\u8207\u6771\u6a02\u66f8\u8cb7\u96f2\u7522\u89aa\u5104\u5f9e\u5009\u5100\u6703\u50b3\u50b7\u502b\u9ad4\u9918\u4fe0\u5075\u5074\u50d1\u5132\u5152\u862d\u95dc\u8208\u990a\u7378\u5167\u5ca1\u5beb\u8ecd\u8fb2\u6c7a\u51cd\u6de8\u6e96\u64ca\u5289\u5247\u5275\u522a\u5225\u5238\u5291\u5287\u8fa6\u52d9\u52d5\u52f5\u52c1\u52de\u52e2\u52f3\u5340\u83ef\u55ae\u8ce3\u76e7\u885b\u5ee0\u5ef3\u6b77\u58d3\u53ad\u5ec2\u7e23\u53c3\u96d9\u8b8a\u758a\u8449\u865f\u5606\u55ce\u555f\u5433\u54e1\u548f\u97ff\u555e\u5629\u559a\u56b4\u5718\u5712\u570d\u5716\u5713\u5834\u58de\u584a\u5805\u58c7\u58e9\u588a\u5815\u8072\u8655\u5099\u5920\u982d\u593e\u596a\u596e\u734e\u5a66\u5abd\u5b0c\u5b78\u5bf6\u5be6\u5bf5\u5be9\u61b2\u5bae\u5bec\u8cd3\u5c0b\u5c0e\u5c07\u723e\u5875\u5617\u5c64\u6b72\u8c48\u5d17\u5d50\u5cf6\u5dba\u5e63\u5e25\u5e2b\u5e33\u5e36\u5e6b\u5ee3\u838a\u6176\u5eab\u61c9\u5edf\u9f90\u5ee2\u958b\u7570\u5f35\u5f37\u6b78\u9304\u5f91\u61b6\u6182\u61f7\u614b\u61d0\u7e3d\u6200\u6046\u60e1\u60f1\u6085\u61f8\u9a5a\u6158\u6163\u6232\u6236\u57f7\u64f4\u6383\u63da\u64fe\u64ab\u6436\u8b77\u5831\u64d4\u64ec\u64c1\u6514\u64c7\u639b\u6311\u6316\u646f\u6399\u63ee\u640d\u63db\u64da\u64f2\u64fb\u652c\u64fa\u6416\u6524\u64a4\u64cd\u64ce\u6536\u6575\u6559\u6578\u65b7\u7121\u820a\u6642\u986f\u66c9\u66ab\u8853\u6a5f\u6bba\u96dc\u6b0a\u689d\u4f86\u694a\u5091\u6975\u69cb\u67a3\u69cd\u6953\u6ac3\u6a19\u68e7\u6a39\u6a23\u6a4b\u6a3a\u5922\u6aa2\u6a13\u6b61\u6b50\u6b98\u6bc6\u7562\u6c23\u6f22\u6e6f\u6e9d\u6c92\u6ec4\u6e88\u6d89\u6cfb\u6f54\u6dfa\u6f3f\u6e2c\u6fdf\u6e3e\u6fc3\u6fe4\u6e26\u6e19\u6f32\u6eab\u7063\u6fd5\u6f70\u6efe\u6eff\u6ffe\u6feb\u701f\u6f5b\u6ec5\u71c8\u9748\u707d\u7149\u71be\u7169\u71b1\u611b\u723a\u727d\u72c0\u7368\u72f9\u7344\u7375\u8c6c\u8c93\u73fe\u74b0\u74bd\u96fb\u756b\u66a2\u7642\u760b\u76e3\u76e4\u76dc\u84cb\u77ef\u7926\u78bc\u78da\u786f\u78ba\u7919\u79ae\u79aa\u96e2\u7a2e\u7a4d\u7a31\u7a05\u7a69\u7aae\u7aca\u7af6\u7b46\u7c60\u7c21\u985e\u7ce7\u7dca\u7cfe\u7d05\u7d04\u7d1a\u7d00\u7d0d\u7e31\u7d1b\u7d19\u7d0b\u7d21\u7d10\u7dda\u7d44\u7d30\u7e54\u7d42\u7d46\u7d93\u7d81\u7d50\u7e5e\u7e6a\u7d66\u7d61\u7d55\u7d71\u7e7c\u7e8c\u7dad\u7d9c\u7da0\u7de8\u7de3\u7e1b\u7e2b\u7e8f\u7e2e\u7e73\u7f50\u7db2\u7f85\u7f70\u7f77\u7f69\u7f6a\u7f72\u7f8e\u7fa4\u7fd4\u8001\u8010\u8077\u806f\u819a\u816b\u8139\u8105\u81bd\u52dd\u81a0\u8108\u81df\u8166\u8173\u812b\u81c9\u9a30\u822c\u8266\u85dd\u7bc0\u8607\u7bc4\u83d6\u85a6\u85e5\u7372\u84ee\u83dc\u83f2\u71df\u843d\u85cd\u85cf\u865b\u87f2\u96d6\u8766\u8755\u88dc\u8868\u88dd\u897f\u898b\u89c0\u898f\u89ba\u89f8\u8a08\u8a02\u8a8d\u8a0e\u8b93\u8a13\u8b70\u8a18\u8b1b\u8a31\u8ad6\u8a2d\u8a2a\u8b49\u8a55\u8b58\u8a34\u8a3a\u8a5e\u8b6f\u8a66\u8a69\u8aa0\u8a71\u8a62\u8a72\u8a73\u8a9e\u8aa4\u8aaa\u8acb\u8af8\u8b80\u8ab2\u8ab0\u8abf\u8ac7\u8b00\u8b0a\u8b1d\u8b5c\u8c37\u8c9d\u8ca0\u8ca1\u8cac\u6557\u8ca8\u8cea\u8cfc\u8cbb\u8cc7\u8ce6\u8ced\u8cf4\u8cfa\u8b9a\u8d08\u8d99\u8d95\u8da8\u8e8d\u8eca\u8ecc\u8f49\u8f2a\u8edf\u8f15\u8f09\u8f03\u8f14\u8f1b\u8f29\u8f1d\u8f2f\u8f38\u8fad\u908a\u9054\u9077\u904e\u904b\u9084\u9032\u9023\u9072\u9069\u9078\u905e\u907a\u9059\u90f5\u9130\u912d\u91c3\u91cb\u88e1\u9451\u92fc\u9470\u9322\u947d\u9435\u9234\u9285\u9280\u92ea\u93c8\u92b7\u9396\u932f\u9326\u9375\u93ae\u93e1\u9577\u9580\u9583\u9589\u554f\u95d6\u9592\u9593\u60b6\u9b27\u805e\u968a\u967d\u9670\u9663\u968e\u969b\u9678\u9673\u964c\u964d\u9650\u96aa\u96a8\u96b1\u96e3\u9727\u975c\u97d3\u9801\u9802\u9805\u9806\u9808\u9867\u9810\u9818\u983b\u984c\u984f\u98a8\u98db\u98ef\u98f2\u98fe\u98fd\u9905\u9928\u9996\u99ac\u9a45\u9a57\u9a0e\u9a19\u9a37\u9aa8\u9ad8\u9b3c\u9b5a\u9ce5\u96de\u9cf4\u9d3b\u9d5d\u9ec3\u9ed1\u9f4a\u9f52\u9f8d";
	int score = candidate.directUtf8 ? 18 : 0;
	unsigned kana = 0;
	unsigned halfwidthKana = 0;
	unsigned simplified = 0;
	unsigned traditional = 0;
	for (auto c : candidate.text) {
		if (c < 0x80) {
			if (c >= 0x20 && c != 0x7F)
				score++;
			else
				score -= 20;
		} else if (c >= 0xFF61 && c <= 0xFF9F) {
			halfwidthKana++;
			score -= 3;
		} else if (c >= 0x3040 && c <= 0x30FF) {
			kana++;
			score += 2;
		}
		else if ((c >= 0x3400 && c <= 0x9FFF)) {
			bool isSimplified = std::wcschr(simplifiedOnly, c) && !std::wcschr(traditionalOnly, c);
			bool isTraditional = std::wcschr(traditionalOnly, c) && !std::wcschr(simplifiedOnly, c);

			score += std::wcschr(commonIdeographs, c) ? 6 : candidate.recoveredFromShiftJis ? 0 : 2;
			if (isSimplified) {
				simplified++;
				score += 5;
			}
			if (isTraditional) {
				traditional++;
				score += 5;
			}
		} else if (c == 0xFFFD || c < 0x20)
			score -= 20;
		else if ((c >= 0x2000 && c <= 0x206F) || (c >= 0xFF01 && c <= 0xFF60))
			score++;
		else
			score -= 2;
	}
	if (candidate.codePage == 932)
		score += static_cast<int>(kana);
	if (candidate.codePage == 936 || candidate.codePage == 54936)
		score += static_cast<int>(simplified) * 3 - static_cast<int>(traditional) * 3;
	if (candidate.codePage == 950)
		score += static_cast<int>(traditional) * 3 - static_cast<int>(simplified) * 3;
	if (candidate.recoveredFromShiftJis &&
		(((candidate.codePage == 936 || candidate.codePage == 54936) && simplified) ||
		(candidate.codePage == 950 && traditional)))
		score += 4;
	if (halfwidthKana * 2 > candidate.text.size())
		score -= 8;
	score += (std::clamp)(candidate.detectorConfidence, 0, 100) / 20;
	if ((country == "cn" && (candidate.codePage == 936 || candidate.codePage == 54936)) ||
		((country == "tw" || country == "hk") && candidate.codePage == 950) ||
		(country == "jp" && candidate.codePage == 932))
		score += 3;
	return score;
}

static std::wstring decodeHostlistName(const std::string &input, const std::string &country)
{
	static std::mutex cacheMutex;
	static std::map<std::pair<std::string, std::string>, std::wstring> cache;
	auto cacheKey = std::make_pair(input, country);
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		auto cached = cache.find(cacheKey);
		if (cached != cache.end())
			return cached->second;
	}

	std::vector<DecodedNameCandidate> candidates;
	auto addCandidates = [&](const std::string &bytes, bool recovered) {
		auto detections = detectCodePages(bytes);
		for (auto codePage : {932U, 936U, 950U, 54936U}) {
			std::wstring decoded;
			std::string roundTrip;
			if (!decodeCodePage(bytes, codePage, 0, decoded))
				continue;
			if (!encodeCodePageExact(decoded, codePage, roundTrip) || roundTrip != bytes)
				continue;
			auto duplicate = std::find_if(candidates.begin(), candidates.end(), [&](const DecodedNameCandidate &value) {
				return value.text == decoded && value.codePage == codePage && value.recoveredFromShiftJis == recovered;
			});
			int confidence = detections.count(codePage) ? detections[codePage] : 0;
			if (duplicate == candidates.end())
				candidates.push_back({std::move(decoded), codePage, confidence, false, recovered});
			else if (confidence > duplicate->detectorConfidence)
				duplicate->detectorConfidence = confidence;
		}
	};

	std::wstring utf8;
	if (decodeCodePage(input, CP_UTF8, MB_ERR_INVALID_CHARS, utf8)) {
		candidates.push_back({utf8, CP_UTF8, 100, true, false});
		std::string recoveredBytes;
		if (encodeCodePageExact(utf8, 932, recoveredBytes) && recoveredBytes != input)
			addCandidates(recoveredBytes, true);
	} else
		addCandidates(input, false);

	std::wstring result = L"\uFFFD";
	int bestScore = INT_MIN;
	for (const auto &candidate : candidates) {
		auto score = scoreDecodedName(candidate, country);
		if (score > bestScore) {
			bestScore = score;
			result = candidate.text;
		}
	}
	{
		std::lock_guard<std::mutex> lock(cacheMutex);
		cache.emplace(std::move(cacheKey), result);
	}
	return result;
}

static std::wstring limitWideStr(const std::wstring &str, unsigned limit)
{
	if (str.size() <= limit)
		return str;
	if (limit <= 3)
		return std::wstring(limit, L'.');

	auto end = limit - 3;
	if (end < str.size() && end > 0 && str[end - 1] >= 0xD800 && str[end - 1] <= 0xDBFF)
		end--;
	return str.substr(0, end) + L"...";
}

SmallHostlist::SmallHostlist(float ratio, SokuLib::Vector2i pos, SokuLib::MenuConnect *parent) :
	_parent(parent),
	_pos(pos),
	_ratio(ratio)
{
	SokuLib::Vector2i size;

	for (unsigned i = 0; i < this->_sprites.size(); i++) {
		auto &sprite = this->_sprites[i];

		if (i == this->_sprites.size() - 2) {
			sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/CRTeffect.png").string().c_str());
			sprite.setSize(sprite.texture.getSize());
		} else {
			if (i == this->_sprites.size() - 1)
				sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/title.png").string().c_str());
			else if (i == this->_sprites.size() - 3)
				sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/hostlistBg.png").string().c_str());
			else
				sprite.texture.loadFromGame(_spritesPaths[i]);
			sprite.setSize((sprite.texture.getSize() * ratio).to<unsigned>());
		}
		sprite.rect.width = sprite.texture.getSize().x;
		sprite.rect.height = sprite.texture.getSize().y;
	}
	this->_sprites[1].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_sprites[2].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_background.emplace_back(new Image(this->_sprites.back(), modifyPos(0, 0)));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[1], modifyPos(-480, 0), modifyPos(0, 0), MAX_OVERLAY_ANIMATION));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[2], modifyPos(640, 0), modifyPos(160, 0), MAX_OVERLAY_ANIMATION));

	this->_foreground.emplace_back(new Image(this->_sprites[0], modifyPos(15, 15)));
	this->_foreground.emplace_back(new Image(this->_sprites[this->_sprites.size() - 2], modifyPos(0, 0)));
	this->_foreground.emplace_back(new Image(this->_sprites[this->_sprites.size() - 3], modifyPos(0, 0)));

	this->_topOverlay.emplace_back(new Image(this->_sprites[24], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(203, -270), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(512, -265), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(177, -205), 0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(245, -233), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(372, -199), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[18], modifyPos(24, -267), -0.003490658503988659));
	this->_topOverlay.emplace_back(new Image(this->_sprites[14], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(419, -236), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(162, -206), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(187, -216), -0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(191, -190), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(217, -194), -0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(307, -206), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(546, -237), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(451, -204), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(362, -224), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(572, -184), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(-47, -223), 0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(540, -216), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(275, -238), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(40, -250), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(-15, -191), 0.005235987755982988));
	this->_topOverlay.emplace_back(new Image(this->_sprites[4], modifyPos(0, -200)));

	this->_botOverlay.emplace_back(new Image(this->_sprites[23], modifyPos(0, 616)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(335, 626), -0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(490, 618), 0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[20], modifyPos(54, 609), 0.0017453292519943296));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(-26, 607), 0.006981317007977318));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(559, 603), -0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(632, 593), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[16], modifyPos(419, 619), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[15], modifyPos(6, 611), -0.0017453292519943296));
	this->_botOverlay.emplace_back(new Image(this->_sprites[13], modifyPos(0, 612)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(435, 636), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(151, 613), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(-37, 626), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(363, 636), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(285, 622), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(467, 668), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(337, 663), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(262, 620), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(109, 658), 0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(83, 647), -0.020943951023931952));
	this->_botOverlay.emplace_back(new Image(this->_sprites[3], modifyPos(0, 632)));
	this->_netThread = std::thread([this]{
		while (this->_running) {
			this->_refreshHostlist();
			std::unique_lock<std::mutex> lock(this->_wakeMutex);

			this->_wakeCondition.wait_for(lock, std::chrono::seconds(10), [this]{
				return !this->_running;
			});
		}
		this->_workerStopped = true;
	});

	this->_playing.texture.createFromText("Playing", lobbyData->getFont(12), {400, 20}, &size);
	this->_playing.setSize(size.to<unsigned>());
	this->_playing.rect.width = size.x;
	this->_playing.rect.height = size.y;
	this->_playing.setPosition(modifyPos(430, 94) + SokuLib::Vector2i{-this->_playing.rect.width / 2, 0});

	this->_hosting.texture.createFromText("Hosting", lobbyData->getFont(12), {400, 20}, &size);
	this->_hosting.setSize(size.to<unsigned>());
	this->_hosting.rect.width = size.x;
	this->_hosting.rect.height = size.y;
	this->_hosting.setPosition(modifyPos(430, 94) + SokuLib::Vector2i{-this->_hosting.rect.width / 2, 0});
}

SmallHostlist::~SmallHostlist()
{
	this->requestStop();
	if (this->_netThread.joinable())
		this->_netThread.join();
	if (this->_errorMsg)
		free(this->_errorMsg);
}

void SmallHostlist::requestStop()
{
	this->_running = false;
	this->_wakeCondition.notify_all();
}

bool SmallHostlist::isStopped() const
{
	return this->_workerStopped;
}

#define CRenderer_Unknown1 ((void (__thiscall *)(int, int))0x404AF0)

void SmallHostlist::_displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size)
{
	SokuLib::Sprite (&CursorSprites)[3] = *(SokuLib::Sprite (*)[3])0x89A6C0;

	//0x443a50 -> Vanilla display cursor
	CursorSprites[0].scale.x = size.x * 0.00195313f * this->_ratio;
	CursorSprites[0].scale.y = size.y / 16.f * this->_ratio;
	pos.x -= 7;
	CursorSprites[0].render(this->_pos.x + pos.x * this->_ratio, this->_pos.y + pos.y * this->_ratio);
	CRenderer_Unknown1(0x896B4C, 2);
	CursorSprites[1].scale.x = this->_ratio;
	CursorSprites[1].scale.y = this->_ratio;
	CursorSprites[1].rotation = *(float *)0x89A450 * 4.f;
	CursorSprites[1].render(this->_pos.x + pos.x * this->_ratio, this->_pos.y + (pos.y + 8.f) * this->_ratio);
	CursorSprites[2].scale.x = this->_ratio;
	CursorSprites[2].scale.y = this->_ratio;
	CursorSprites[2].rotation = -*(float *)0x89A450 * 4.f;
	CursorSprites[2].render(this->_pos.x + (pos.x - 14.f) * this->_ratio, this->_pos.y + (pos.y - 1.f) * this->_ratio);
	CRenderer_Unknown1(0x896B4C, 1);
	CursorSprites[0].scale.x = 1;
	CursorSprites[0].scale.y = 1;
	CursorSprites[1].scale.x = 1;
	CursorSprites[1].scale.y = 1;
	CursorSprites[2].scale.x = 1;
	CursorSprites[2].scale.y = 1;
}

void SmallHostlist::_applyPendingEntries()
{
	std::vector<std::unique_ptr<HostEntry>> oldHostEntries;
	std::vector<std::unique_ptr<PlayEntry>> oldPlayEntries;
	std::string selectedIp;
	unsigned short selectedPort = 0;
	bool newHost = false;
	{
		std::lock_guard<std::mutex> lock(this->_entriesMutex);

		if (!this->_hasPendingEntries)
			return;
		if (this->_spectator) {
			if (this->_hostSelect < this->_playEntries.size()) {
				selectedIp = this->_playEntries[this->_hostSelect]->ip;
				selectedPort = this->_playEntries[this->_hostSelect]->port;
			}
		} else if (this->_hostSelect < this->_hostEntries.size()) {
			selectedIp = this->_hostEntries[this->_hostSelect]->ip;
			selectedPort = this->_hostEntries[this->_hostSelect]->port;
		}
		oldHostEntries.swap(this->_hostEntries);
		oldPlayEntries.swap(this->_playEntries);
		this->_hostEntries.swap(this->_pendingHostEntries);
		this->_playEntries.swap(this->_pendingPlayEntries);
		this->_hasPendingEntries = false;
		newHost = this->_pendingNewHost;
		this->_pendingNewHost = false;
	}

	auto findSelection = [&](const auto &entries) {
		if (!selectedIp.empty()) {
			auto selected = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
				return entry->ip == selectedIp && entry->port == selectedPort;
			});

			if (selected != entries.end())
				return static_cast<unsigned>(std::distance(entries.begin(), selected));
		}
		return entries.empty() ? 0u : (std::min)(this->_hostSelect, static_cast<unsigned>(entries.size() - 1));
	};
	this->_hostSelect = this->_spectator ? findSelection(this->_playEntries) : findSelection(this->_hostEntries);
	if (this->_playEntries.size() <= MAX_VISIBLE_ENTRIES)
		this->_playOffset = 0;
	else if (this->_playOffset + MAX_VISIBLE_ENTRIES > this->_playEntries.size())
		this->_playOffset = static_cast<unsigned>(this->_playEntries.size() - MAX_VISIBLE_ENTRIES);
	if (this->_hostEntries.size() <= MAX_VISIBLE_ENTRIES)
		this->_hostOffset = 0;
	else if (this->_hostOffset + MAX_VISIBLE_ENTRIES > this->_hostEntries.size())
		this->_hostOffset = static_cast<unsigned>(this->_hostEntries.size() - MAX_VISIBLE_ENTRIES);
	if (newHost)
		playSound(49);
}

bool SmallHostlist::update()
{
	this->_applyPendingEntries();
	auto adjustOffset = [this](bool spectator){
		auto size = spectator ? this->_playEntries.size() : this->_hostEntries.size();
		auto &offset = spectator ? this->_playOffset : this->_hostOffset;

		if (size == 0) {
			offset = 0;
			return;
		}
		if (size <= MAX_VISIBLE_ENTRIES) {
			offset = 0;
			return;
		}
		if (this->_hostSelect >= size)
			this->_hostSelect = static_cast<unsigned>(size - 1);
		if (this->_hostSelect >= offset + MAX_VISIBLE_ENTRIES)
			offset = this->_hostSelect - MAX_VISIBLE_ENTRIES + 1;
		if (this->_hostSelect < offset)
			offset = this->_hostSelect;
	};

	if (this->_overlayTimer <= MAX_OVERLAY_ANIMATION) {
		int translate = (200 * this->_ratio) * std::pow((float)this->_overlayTimer / MAX_OVERLAY_ANIMATION, 2);

		if (this->_overlayTimer == 5)
			playSound(61);
		for (auto &elem : this->_topOverlay)
			elem->translate.y = translate;
		for (auto &elem : this->_botOverlay)
			elem->translate.y = 1 - translate;
		this->_overlayTimer++;
	}
	this->_errorMutex.lock();
	if (this->_errorMsg) {
		auto errorMsg = this->_errorMsg;
		SokuLib::Vector2i size{0, 0};

		this->_errorMsg = nullptr;
		this->_errorMutex.unlock();
		if (*errorMsg)
			this->_error.texture.createFromText(("Refresh error: " + std::string(errorMsg)).c_str(), lobbyData->getFont(12), {540, 20}, &size);
		else
			this->_error.texture.destroy();
		free(errorMsg);
		this->_error.setSize(size.to<unsigned>());
		this->_error.rect.width = size.x;
		this->_error.rect.height = size.y;
		this->_error.tint = SokuLib::Color::Red;
		this->_error.setPosition(modifyPos(320, 480) + SokuLib::Vector2i{-size.x / 2, -16});
	} else
		this->_errorMutex.unlock();
	if (this->_parent->choice > 0) {
		if (
			this->_parent->subchoice == 5 || //Already Playing
			this->_parent->subchoice == 10   //Connect Failed
		) {
			*(*(char **)0x89a390 + 20) = false;
			this->_parent->choice = 0;
			this->_parent->subchoice = 0;
			playSound(0x29);
		}
	}
	for (auto &elem : this->_background)
		elem->update();
	for (auto &elem : this->_topOverlay)
		elem->update();
	for (auto &elem : this->_botOverlay)
		elem->update();
	if (this->_overlayTimer <= 5)
		return true;
	for (auto &elem : this->_foreground)
		elem->update();
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1) {
		this->_spectator = !this->_spectator;
		this->_hostSelect = 0;
		this->_hostOffset = 0;
		this->_playOffset = 0;
		playSound(0x27);
		return true;
	}
	if (!this->_selected) {
		if (SokuLib::inputMgrs.input.b == 1)
			return false;
		if (SokuLib::inputMgrs.input.a == 1) {
			if (this->_selection)
				return false;
			else {
				this->_selected = true;
				playSound(0x28);
				return true;
			}
		}
		if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1) {
			this->_selection = !this->_selection;
			playSound(0x27);
			return true;
		}
	} else if (!this->_parent->choice) {
		if (SokuLib::inputMgrs.input.b == 1) {
			this->_selected = false;
			playSound(0x29);
			return true;
		}
		if (this->_spectator ? this->_playEntries.empty() : this->_hostEntries.empty())
			return true;
		if (SokuLib::inputMgrs.input.a == 1) {
			this->_parent->joinHost(
				(this->_spectator ? this->_playEntries[this->_hostSelect]->ip : this->_hostEntries[this->_hostSelect]->ip).c_str(),
				this->_spectator ? this->_playEntries[this->_hostSelect]->port : this->_hostEntries[this->_hostSelect]->port,
				this->_spectator
			);
			playSound(0x28);
			return true;
		}

		auto axis = std::abs(SokuLib::inputMgrs.input.verticalAxis);

		if (axis == 1 || (axis >= 36 && axis % 6 == 0)) {
			auto size = this->_spectator ? this->_playEntries.size() : this->_hostEntries.size();

			if (!size)
				return true;
			if (SokuLib::inputMgrs.input.verticalAxis > 0)
				this->_hostSelect = (this->_hostSelect + 1) % size;
			else if (this->_hostSelect == 0)
				this->_hostSelect = static_cast<unsigned>(size - 1);
			else
				this->_hostSelect--;
			adjustOffset(this->_spectator);
			playSound(0x27);
			return true;
		}
	}
	return true;
}

void SmallHostlist::render()
{
	for (auto &elem : this->_background)
		elem->render();
	for (auto &elem : this->_topOverlay)
		elem->render();
	for (auto &elem : this->_botOverlay)
		elem->render();
	if (this->_overlayTimer <= 5)
		return;
	if (!this->_selected)
		this->_displaySokuCursor({62, this->_selection ? 364 : 160}, {160, 16});
	for (auto &elem : this->_foreground)
		elem->render();
	this->_entriesMutex.lock();
	(&this->_hosting)[this->_spectator].draw();
	if (!this->_spectator) {
		auto start = (std::min)(this->_hostOffset, static_cast<unsigned>(this->_hostEntries.size() > 0 ? this->_hostEntries.size() - 1 : 0));
		auto end = (std::min)(this->_hostEntries.size(), static_cast<size_t>(start + MAX_VISIBLE_ENTRIES));

		for (unsigned i = start; i < end; i++) {
			auto row = i - start;
			auto &entry = *this->_hostEntries[i];
			auto flag = lobbyData->flags.find(entry.country);

			if (this->_hostSelect == i && this->_selected)
				this->_displaySokuCursor({262, static_cast<int>(118 + row * 16 / this->_ratio)}, {375, static_cast<unsigned>(16 / this->_ratio)});
			if (flag == lobbyData->flags.end())
				flag = lobbyData->flags.find("default");
			flag->second->setPosition(modifyPos(266, 118) + SokuLib::Vector2i{0, static_cast<int>(row * 16)});
			flag->second->setSize({16, 16});
			flag->second->draw();

			if (!entry.name.texture.hasTexture()) {
				SokuLib::Vector2i size;
				int texId = 0;

				if (!createTextTexture(texId, convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(entry.nameStr).c_str(), lobbyData->getFont(12), {400, 20}, &size))
					puts("Error creating text texture");
				entry.name.texture.setHandle(texId, {400, 20});
				entry.name.setSize(size.to<unsigned>());
				entry.name.rect.width = size.x;
				entry.name.rect.height = size.y;

				if (!createTextTexture(texId, convertEncoding<char, wchar_t, UTF8Decode, UTF16Encode>(entry.msgStr).c_str(), lobbyData->getFont(12), {400, 20}, &size))
					puts("Error creating text texture");
				entry.msg.texture.setHandle(texId, {400, 20});
				entry.msg.setSize(size.to<unsigned>());
				entry.msg.rect.width = size.x;
				entry.msg.rect.height = size.y;
			}
			entry.name.setPosition(modifyPos(262, 120) + SokuLib::Vector2i{16, static_cast<int>(row * 16)});
			entry.name.draw();
			entry.msg.setPosition(modifyPos(402, 120) + SokuLib::Vector2i{16, static_cast<int>(row * 16)});
			entry.msg.draw();
		}
	}
	if (this->_spectator) {
		auto start = (std::min)(this->_playOffset, static_cast<unsigned>(this->_playEntries.size() > 0 ? this->_playEntries.size() - 1 : 0));
		auto end = (std::min)(this->_playEntries.size(), static_cast<size_t>(start + MAX_VISIBLE_ENTRIES));

		for (unsigned i = start; i < end; i++) {
			auto row = i - start;
			auto &entry = *this->_playEntries[i];
			auto flag1 = lobbyData->flags.find(entry.country1);
			auto flag2 = lobbyData->flags.find(entry.country2);

			if (this->_hostSelect == i && this->_selected)
				this->_displaySokuCursor({266, static_cast<int>(118 + row * 16 / this->_ratio)}, {375, static_cast<unsigned>(16 / this->_ratio)});
			if (!entry.names.texture.hasTexture()) {
				SokuLib::Vector2i size;
				int texId = 0;

				if (!createTextTexture(texId, entry.namesStr.c_str(), lobbyData->getFont(12), {400, 20}, &size))
					puts("Error creating text texture");
				entry.names.texture.setHandle(texId, {400, 20});
				entry.names.setSize(size.to<unsigned>());
				entry.names.rect.width = size.x;
				entry.names.rect.height = size.y;
			}
			entry.names.setPosition(modifyPos(430, 120) + SokuLib::Vector2i{ - entry.names.rect.width / 2, static_cast<int>(row * 16)});
			entry.names.draw();

			if (flag1 == lobbyData->flags.end())
				flag1 = lobbyData->flags.find("default");
			flag1->second->setPosition(entry.names.getPosition() - SokuLib::Vector2i{18, 0});
			flag1->second->setSize({16, 16});
			flag1->second->draw();
			if (flag2 == lobbyData->flags.end())
				flag2 = lobbyData->flags.find("default");
			flag2->second->setPosition(entry.names.getPosition() + SokuLib::Vector2i{2 + entry.names.rect.width, 0});
			flag2->second->setSize({16, 16});
			flag2->second->draw();

			if (!entry.p1chr.empty() && !entry.p2chr.empty()) {
				auto chr1 = lobbyData->emotesByName.find(entry.p1chr + "1");
				auto chr2 = lobbyData->emotesByName.find(entry.p2chr + "1");
				LobbyData::Emote *p1;
				LobbyData::Emote *p2;

				if (chr1 == lobbyData->emotesByName.end()) {
					printf("Unknown emote %s1", entry.p1chr.c_str());
					p1 = &lobbyData->emotes[0];
				} else
					p1 = chr1->second;
				if (chr2 == lobbyData->emotesByName.end()) {
					printf("Unknown emote %s1", entry.p2chr.c_str());
					p2 = &lobbyData->emotes[0];
				} else
					p2 = chr2->second;
				p1->sprite.setSize({16, 16});
				p1->sprite.setPosition(entry.names.getPosition() - SokuLib::Vector2i{36, 0});
				p1->sprite.draw();
				p2->sprite.setSize({16, 16});
				p2->sprite.setPosition(entry.names.getPosition() + SokuLib::Vector2i{20 + entry.names.rect.width, 0});
				p2->sprite.draw();
			}
		}
	}
	this->_entriesMutex.unlock();
	this->_error.draw();
}

static std::string limitStr(const std::string &str, unsigned limit)
{
	if (str.size() <= limit)
		return str;
	return str.substr(0, limit - 3) + "...";
}

void SmallHostlist::_refreshHostlist()
{
	if (!this->_running)
		return;
	try {
		std::set<std::pair<std::string, unsigned short>> knownHosts;
		{
			std::lock_guard<std::mutex> lock(this->_entriesMutex);

			for (const auto &entry : this->_hostEntries)
				knownHosts.emplace(entry->ip, entry->port);
			for (const auto &entry : this->_pendingHostEntries)
				knownHosts.emplace(entry->ip, entry->port);
		}
		if (!this->_running)
			return;
		auto response = lobbyData->httpRequest(
			"https://konni.delthas.fr/games",
			"GET",
			"",
			20000L,
			&this->_running
		);
		if (!this->_running)
			return;
		nlohmann::json val = nlohmann::json::parse(response);
		std::vector<std::unique_ptr<HostEntry>> hostEntries;
		std::vector<std::unique_ptr<PlayEntry>> playEntries;
		bool newHost = false;

		for (auto &j : val) {
			if (!this->_running)
				return;
			if (j["started"]) {
				if (!j["spectatable"])
					continue;

				auto entry = std::make_unique<PlayEntry>();

				entry->p1chr = j["host_character"];
				entry->p2chr = j["client_character"];
				entry->country1 = j["host_country"];
				entry->country2 = j["client_country"];
				entry->namesStr = limitWideStr(decodeHostlistName(j["host_name"].get<std::string>(), entry->country1), 10) +
					L"|" + limitWideStr(decodeHostlistName(j["client_name"].get<std::string>(), entry->country2), 10);
				entry->ip = j["ip"];
				entry->port = std::stoul(entry->ip.substr(entry->ip.find_last_of(':') + 1));
				entry->ip = entry->ip.substr(0, entry->ip.find_last_of(':'));

				auto it = std::find_if(playEntries.begin(), playEntries.end(), [&entry](const std::unique_ptr<PlayEntry> &a){
					return a->ip == entry->ip && a->port == entry->port;
				});

				if (it == playEntries.end())
					playEntries.emplace_back(std::move(entry));
				else
					*it = std::move(entry);
			} else {
				auto entry = std::make_unique<HostEntry>();

				entry->nameStr = convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(
					limitWideStr(decodeHostlistName(j["host_name"].get<std::string>(), j["host_country"].get<std::string>()), 16)
				);
				entry->msgStr = limitStr(j["message"], 16);
				entry->country = j["host_country"];
				entry->ap = j["autopunch"];
				entry->ranked = j["ranked"];
				entry->ip = j["ip"];
				entry->port = std::stoul(entry->ip.substr(entry->ip.find_last_of(':') + 1));
				entry->ip = entry->ip.substr(0, entry->ip.find_last_of(':'));

				auto it = std::find_if(hostEntries.begin(), hostEntries.end(), [&entry](const std::unique_ptr<HostEntry> &a){
					return a->ip == entry->ip && a->port == entry->port;
				});

				if (it == hostEntries.end()) {
					newHost |= !knownHosts.count({entry->ip, entry->port});
					hostEntries.emplace_back(std::move(entry));
				} else
					*it = std::move(entry);
			}
		}
		if (!this->_running)
			return;
		{
			std::lock_guard<std::mutex> lock(this->_entriesMutex);

			this->_pendingHostEntries = std::move(hostEntries);
			this->_pendingPlayEntries = std::move(playEntries);
			this->_hasPendingEntries = true;
			this->_pendingNewHost |= newHost;
		}
		this->_errorMutex.lock();
		if (this->_errorMsg)
			free(this->_errorMsg);
		this->_errorMsg = strdup("");
		this->_errorMutex.unlock();
	} catch (std::exception &e) {
		if (!this->_running)
			return;
		this->_errorMutex.lock();
		if (this->_errorMsg)
			free(this->_errorMsg);
		this->_errorMsg = strdup(e.what());
		this->_errorMutex.unlock();
		printf("Failed to refresh hostlist: %s\n", e.what());
	}
}

SmallHostlist::Image::Image(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos) :
	_sprite(sprite),
	_pos(pos)
{
}

void SmallHostlist::Image::update()
{
}

void SmallHostlist::Image::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.draw();
}

SmallHostlist::ScrollingImage::ScrollingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i startPos, SokuLib::Vector2i endPos, unsigned int animationDuration) :
	_sprite(sprite),
	_startPos(startPos),
	_endPos(endPos),
	_animationDuration(animationDuration)
{
}

void SmallHostlist::ScrollingImage::update()
{
	if (this->_animationDuration <= this->_animationCtr)
		return;
	this->_animationCtr++;
}

void SmallHostlist::ScrollingImage::render()
{
	this->_sprite.setPosition(this->_startPos + (this->_endPos - this->_startPos) * std::pow((float)this->_animationCtr / this->_animationDuration, 2) + this->translate);
	this->_sprite.draw();
}

SmallHostlist::RotatingImage::RotatingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos, float anglePerFrame) :
	_sprite(sprite),
	_pos(pos),
	_anglePerFrame(anglePerFrame)
{

}

void SmallHostlist::RotatingImage::update()
{
	this->_rotation = std::fmod(this->_rotation + this->_anglePerFrame, 2 * M_PI);
}

void SmallHostlist::RotatingImage::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.setRotation(this->_rotation);
	this->_sprite.draw();
}
