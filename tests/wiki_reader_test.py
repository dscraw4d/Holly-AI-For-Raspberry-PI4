import io
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import wiki_reader


class ReaderTests(unittest.TestCase):
    def setUp(self):
        self.payload = {
            "type": "standard",
            "extract": "Red Dwarf is a British television sitcom. It was created in 1988.",
            "content_urls": {"desktop": {"page": "https://en.wikipedia.org/wiki/Red_Dwarf"}},
        }

    def test_prepared_command(self):
        self.assertEqual(wiki_reader.prepare("Red Dwarf", self.payload),
                         "wiki import Red Dwarf is a British television sitcom. | https://en.wikipedia.org/wiki/Red_Dwarf")

    def test_rejects_invalid_sources_and_article_types(self):
        for source in ("http://en.wikipedia.org/wiki/A", "https://en.wikipedia.org.evil/wiki/A",
                       "https://en.wikipedia.org/wiki/A?x=1", "https://en.wikipedia.org/wiki/A/B"):
            self.payload["content_urls"]["desktop"]["page"] = source
            with self.assertRaises(ValueError):
                wiki_reader.prepare("A", self.payload)
        self.payload["content_urls"]["desktop"]["page"] = "https://en.wikipedia.org/wiki/A"
        self.payload["type"] = "disambiguation"
        with self.assertRaises(ValueError):
            wiki_reader.prepare("A", self.payload)

    def test_truncation_and_rejects_separator(self):
        self.payload["extract"] = "Long " * 70 + "."
        self.assertTrue(wiki_reader.excerpt(self.payload["extract"]).endswith("..."))
        self.assertLessEqual(len(wiki_reader.excerpt(self.payload["extract"])), 175)
        for opening in ("A | pipe.", ""):
            self.payload["extract"] = opening
            with self.assertRaises(ValueError):
                wiki_reader.prepare("A", self.payload)

    def test_ssh_argument_list_and_dry_run(self):
        with patch.object(wiki_reader, "fetch", return_value=self.payload), \
             patch.object(wiki_reader.subprocess, "run") as run:
            run.return_value.returncode = 0
            self.assertEqual(wiki_reader.main(["--host", "192.168.1.20", "--title", "Red Dwarf"]), 0)
            args = run.call_args.args[0]
            self.assertEqual(args[:4], ["ssh", "-o", "RekeyLimit=4M", "holly@192.168.1.20"])
            self.assertTrue(args[4].startswith("wiki import "))
            run.reset_mock()
            with patch("sys.stdout", new_callable=io.StringIO) as output:
                self.assertEqual(wiki_reader.main(["--host", "192.168.1.20", "--title", "Red Dwarf", "--dry-run"]), 0)
                self.assertIn("Red Dwarf", output.getvalue())
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
