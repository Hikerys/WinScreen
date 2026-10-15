import os
import sys
import json
import unittest
from unittest.mock import patch, MagicMock

SRC_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)

# Mock tkinter for headless / Linux CI environments without libtk
for mod in ["tkinter", "tkinter.ttk", "tkinter.filedialog", "tkinter.messagebox"]:
    if mod not in sys.modules:
        sys.modules[mod] = MagicMock()

import gui



class TestGuiUpdateLogic(unittest.TestCase):
    def test_version_parsing(self):
        self.assertEqual(gui.parse_version("1.2.1"), (1, 2, 1))
        self.assertEqual(gui.parse_version("v1.3.1"), (1, 3, 1))
        self.assertTrue(gui.is_newer_version("v1.3.0", "1.2.1"))
        self.assertFalse(gui.is_newer_version("v1.2.1", "1.2.1"))
        self.assertFalse(gui.is_newer_version("v1.2.0", "1.2.1"))
        self.assertFalse(gui.is_newer_version("v1.1.9", "1.2.1"))

    @patch("urllib.request.urlopen")
    @patch("gui.is_arm_architecture", return_value=False)
    def test_check_updates_x86_64(self, mock_is_arm, mock_urlopen):
        mock_data = {
            "tag_name": "v1.3.0",
            "html_url": "https://github.com/Hikerys/WinScreen/releases/tag/v1.3.0",
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
        }
        mock_resp = MagicMock()
        mock_resp.read.return_value = json.dumps(mock_data).encode("utf-8")
        mock_resp.__enter__.return_value = mock_resp
        mock_urlopen.return_value = mock_resp

        info = gui.check_for_updates()
        self.assertIsNotNone(info)
        self.assertTrue(info["has_update"])
        self.assertEqual(info["latest_version"], "v1.3.0")
        self.assertEqual(info["download_url"], "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen.exe")
        self.assertFalse(info["is_arm"])

    @patch("urllib.request.urlopen")
    @patch("gui.is_arm_architecture", return_value=True)
    def test_check_updates_arm64(self, mock_is_arm, mock_urlopen):
        mock_data = {
            "tag_name": "v1.3.0",
            "html_url": "https://github.com/Hikerys/WinScreen/releases/tag/v1.3.0",
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
        }
        mock_resp = MagicMock()
        mock_resp.read.return_value = json.dumps(mock_data).encode("utf-8")
        mock_resp.__enter__.return_value = mock_resp
        mock_urlopen.return_value = mock_resp

        info = gui.check_for_updates()
        self.assertIsNotNone(info)
        self.assertTrue(info["has_update"])
        self.assertEqual(info["latest_version"], "v1.3.0")
        self.assertEqual(info["download_url"], "https://github.com/Hikerys/WinScreen/releases/download/v1.3.0/WinScreen-ARM.exe")
        self.assertTrue(info["is_arm"])


if __name__ == "__main__":
    unittest.main()
