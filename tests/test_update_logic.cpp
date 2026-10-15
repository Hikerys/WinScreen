#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cctype>

// Helper to simulate Utf8ToWide without Windows API in pure C++
static std::wstring Utf8ToWide(const std::string& utf8) {
    std::wstring w(utf8.begin(), utf8.end());
    return w;
}

static std::wstring ExtractExeDownloadUrl(const std::string& json, bool isArm) {
    std::string needle = "\"browser_download_url\"";
    size_t pos = 0;
    std::vector<std::string> armUrls;
    std::vector<std::string> x64Urls;

    while ((pos = json.find(needle, pos)) != std::string::npos) {
        pos += needle.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
            pos++;
        }
        if (pos < json.length() && json[pos] == '\"') {
            pos++;
            size_t endPos = json.find('\"', pos);
            if (endPos != std::string::npos) {
                std::string url = json.substr(pos, endPos - pos);
                if (url.find(".exe") != std::string::npos) {
                    size_t slashPos = url.rfind('/');
                    std::string filename = (slashPos != std::string::npos) ? url.substr(slashPos + 1) : url;
                    std::string lowerName = filename;
                    for (char& c : lowerName) c = (char)tolower((unsigned char)c);

                    if (lowerName.find("arm") != std::string::npos || lowerName.find("aarch64") != std::string::npos) {
                        armUrls.push_back(url);
                    } else {
                        x64Urls.push_back(url);
                    }
                }
                pos = endPos + 1;
            }
        }
    }

    if (isArm) {
        if (!armUrls.empty()) {
            for (const auto& u : armUrls) {
                std::string lower = u;
                for (char& c : lower) c = (char)tolower((unsigned char)c);
                if (lower.find("winscreen-arm.exe") != std::string::npos) return Utf8ToWide(u);
            }
            return Utf8ToWide(armUrls[0]);
        }
    } else {
        if (!x64Urls.empty()) {
            for (const auto& u : x64Urls) {
                std::string lower = u;
                for (char& c : lower) c = (char)tolower((unsigned char)c);
                if (lower.find("winscreen.exe") != std::string::npos && lower.find("arm") == std::string::npos) {
                    return Utf8ToWide(u);
                }
            }
            return Utf8ToWide(x64Urls[0]);
        }
    }
    return L"";
}

int main() {
    std::cout << "[TEST] Running C++ update logic tests..." << std::endl;

    // Test 1: JSON containing both x86_64 and ARM assets
    std::string jsonBoth = R"({
        "tag_name": "v1.3.0",
        "assets": [
            {
                "name": "WinScreen.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe"
            },
            {
                "name": "WinScreen-ARM.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe"
            }
        ]
    })";

    std::wstring x64Url = ExtractExeDownloadUrl(jsonBoth, false);
    std::wstring armUrl = ExtractExeDownloadUrl(jsonBoth, true);

    assert(x64Url == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe");
    assert(armUrl == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe");
    std::cout << "  ✓ Test 1 passed: JSON with both assets correctly resolves for x86_64 and ARM" << std::endl;

    // Test 2: JSON with reverse asset ordering (ARM first, x86_64 second)
    std::string jsonReverse = R"({
        "tag_name": "v1.3.0",
        "assets": [
            {
                "name": "WinScreen-ARM.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe"
            },
            {
                "name": "WinScreen.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe"
            }
        ]
    })";

    std::wstring x64UrlRev = ExtractExeDownloadUrl(jsonReverse, false);
    std::wstring armUrlRev = ExtractExeDownloadUrl(jsonReverse, true);

    assert(x64UrlRev == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe");
    assert(armUrlRev == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe");
    std::cout << "  ✓ Test 2 passed: Reverse order assets resolve correctly for each architecture" << std::endl;

    // Test 3: JSON with only x86_64 asset
    std::string jsonX64Only = R"({
        "tag_name": "v1.3.0",
        "assets": [
            {
                "name": "WinScreen.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe"
            }
        ]
    })";

    assert(ExtractExeDownloadUrl(jsonX64Only, false) == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe");
    assert(ExtractExeDownloadUrl(jsonX64Only, true) == L"");
    std::cout << "  ✓ Test 3 passed: ARM query returns empty (triggers fallback) when only x86_64 asset exists" << std::endl;

    // Test 4: JSON with only ARM asset
    std::string jsonArmOnly = R"({
        "tag_name": "v1.3.0",
        "assets": [
            {
                "name": "WinScreen-ARM.exe",
                "browser_download_url": "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe"
            }
        ]
    })";

    assert(ExtractExeDownloadUrl(jsonArmOnly, false) == L"");
    assert(ExtractExeDownloadUrl(jsonArmOnly, true) == L"https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe");
    std::cout << "  ✓ Test 4 passed: x86_64 query returns empty when only ARM asset exists" << std::endl;

    std::cout << "[SUCCESS] All C++ update tests passed!" << std::endl;
    return 0;
}
