import sqlite3
import unittest
from unittest.mock import Mock, patch

from bot import APIError, Bot


CHAT = -100123
AUTHOR = 21
REPO = "mokagram/mokagram"
ISSUE_URL = f"https://github.com/{REPO}/issues/8"


class IssueBotTest(unittest.TestCase):
    def setUp(self):
        database = sqlite3.connect(":memory:")
        self.addCleanup(database.close)
        self.bot = Bot({
            "TELEGRAM_BOT_TOKEN": "123:fake",
            "TELEGRAM_DEV_CHAT_ID": str(CHAT),
            "GITHUB_TOKEN": "fake",
            "OPENAI_API_KEY": "fake",
        }, database)
        self.bot.username = "moka_issue_bot"
        self.bot.telegram = Mock()
        self.bot.github = Mock(return_value={
            "number": 8, "html_url": ISSUE_URL, "labels": [{"name": "bug"}],
        })
        self.bot.github_labels = Mock(return_value=["bug", "enhancement", "good first issue"])
        self.model = Mock(return_value={
            "title": "Панель пропадает", "body": "## Шаги\n1. Свернуть клиент.",
            "labels": ["enhancement"], "question": "",
        })
        patcher = patch("bot.model_draft", self.model)
        patcher.start()
        self.addCleanup(patcher.stop)

    def message(self):
        return {
            "chat": {"id": CHAT}, "message_id": 12, "from": {"id": AUTHOR},
            "text": "📌 @moka_issue_bot #bug",
            "entities": [{"type": "mention", "offset": 3, "length": 15}],
            "reply_to_message": {"caption": "Панель пропадает после сворачивания клиента"},
            "is_topic_message": True, "message_thread_id": 4,
        }

    def callback(self, action="create", author=AUTHOR):
        return {
            "id": "callback", "data": f"{action}:12", "from": {"id": author},
            "message": {"chat": {"id": CHAT}, "message_id": 13},
        }

    def test_mention_with_reply_previews_then_creates_exactly_once(self):
        self.bot.handle_message(self.message())
        self.bot.handle_message(self.message())
        self.bot.github.assert_not_called()
        self.model.assert_called_once()
        self.assertEqual(
            self.model.call_args.args[2:4],
            ("📌", "Панель пропадает после сворачивания клиента"),
        )
        preview = self.bot.telegram.call_args.kwargs
        self.assertEqual(preview["message_thread_id"], 4)
        self.assertEqual(preview["reply_parameters"], {"message_id": 12})
        self.assertIn("Метки: bug", preview["text"])
        self.bot.handle_callback(self.callback())
        self.bot.handle_callback(self.callback())
        self.bot.github.assert_called_once_with("/issues", data={
            "title": "Панель пропадает", "body": "## Шаги\n1. Свернуть клиент.", "labels": ["bug"],
        })
        self.assertIn(ISSUE_URL, self.bot.telegram.call_args.kwargs["text"])
        self.assertIsNone(self.bot.get_draft(12)["payload"])

    def test_ignores_other_chat_users_and_other_commands(self):
        self.bot.handle_message({**self.message(), "chat": {"id": -42}})
        self.bot.users = {77}
        self.bot.handle_message(self.message())
        self.bot.users = set()
        command = "/issue@another_bot"
        self.bot.handle_message({
            **self.message(), "text": command,
            "entities": [{"type": "bot_command", "offset": 0, "length": len(command)}],
        })
        self.bot.telegram.assert_not_called()
        self.model.assert_not_called()

    def test_uses_model_labels_when_no_hashtags(self):
        command = "/issue@moka_issue_bot"
        self.bot.handle_message({
            **self.message(), "text": command,
            "entities": [{"type": "bot_command", "offset": 0, "length": len(command)}],
        })
        self.assertIn("Метки: enhancement", self.bot.telegram.call_args.kwargs["text"])

    def test_unknown_labels_do_not_invoke_model(self):
        self.bot.handle_message({**self.message(), "text": "📌 @moka_issue_bot #missing"})
        self.assertIn("#missing", self.bot.telegram.call_args.kwargs["text"])
        self.model.assert_not_called()

    def test_unclear_requirements_get_a_question_without_publish_buttons(self):
        self.model.return_value["question"] = "После какого действия это происходит?"
        self.bot.handle_message(self.message())
        self.assertIn(self.model.return_value["question"], self.bot.telegram.call_args.kwargs["text"])
        self.assertNotIn("reply_markup", self.bot.telegram.call_args.kwargs)
        self.assertIsNone(self.bot.get_draft(12))

    def test_only_author_can_publish_or_cancel(self):
        self.bot.handle_message(self.message())
        self.bot.handle_callback(self.callback(author=77))
        self.assertEqual(self.bot.get_draft(12)["state"], "pending")
        self.bot.handle_callback(self.callback("cancel"))
        self.bot.handle_callback(self.callback())
        self.bot.github.assert_not_called()
        self.assertEqual(self.bot.get_draft(12)["state"], "cancelled")

    def test_definitive_github_rejection_can_retry(self):
        self.bot.handle_message(self.message())
        self.bot.github.side_effect = APIError("GitHub", 403)
        self.bot.handle_callback(self.callback())
        self.assertEqual(self.bot.get_draft(12)["state"], "pending")
        self.bot.github.side_effect = None
        self.bot.handle_callback(self.callback())
        self.assertEqual(self.bot.get_draft(12)["state"], "published")

    def test_ambiguous_failure_never_resends_create(self):
        self.bot.handle_message(self.message())
        self.bot.github.side_effect = APIError("GitHub", 503)
        self.bot.handle_callback(self.callback())
        self.bot.handle_callback(self.callback())
        self.bot.github.assert_called_once()
        self.assertIn("Проверьте список Issues", self.bot.telegram.call_args.kwargs["text"])


if __name__ == "__main__":
    unittest.main()
