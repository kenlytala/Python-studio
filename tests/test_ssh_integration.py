"""Optional real encrypted AF_UNIX SSH/SFTP QA of the genuine C controller.

No window, screenshot, TCP listener or external host. Paramiko is QA-only.
"""

from pathlib import Path
import importlib.util
import json
import os
import selectors
import shutil
import subprocess
import tempfile
import time
import unittest

DRIVER = Path(os.environ.get("PRISME_NATIVE_DRIVER", "/missing/ssh_driver"))
HELPER = DRIVER.with_name("python-studio-ssh-helper")
ENABLED = (
    os.environ.get("PRISME_SSH_INTEGRATION") == "1"
    and DRIVER.is_file()
    and HELPER.is_file()
    and importlib.util.find_spec("paramiko")
    and shutil.which("sshfs")
    and Path("/usr/lib/openssh/sftp-server").is_file()
)


@unittest.skipUnless(
    ENABLED, "Native SSH QA needs compiled driver/helper, SSHFS and QA-only Paramiko"
)
class NativeSSHIntegration(unittest.TestCase):
    def setUp(self):
        from ssh_fixture import UnixSSHServer

        self.temp = tempfile.TemporaryDirectory(prefix="prisme-c-real-ssh-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.server = UnixSSHServer(self.root, mode="password")
        self.server.expected_user = "odoo"
        self.addCleanup(self.server.close)
        self.remote = self.root / "remote folder é"
        self.remote.mkdir()
        (self.remote / "main.py").write_text("print('native SSH')\n")
        self.outside = self.root / "outside-sentinel"
        self.outside.write_text("never changed")
        (self.remote / "outside-link").symlink_to(self.outside)
        self.local = self.root / "local"
        self.local.mkdir()
        self.profiles = self.root / "prefs" / "ssh-profiles.json"
        self.events = []

    def run_native(
        self,
        mode="crud",
        accept=True,
        cancel=False,
        wrong=False,
        key="",
        extra_env=None,
    ):
        env = dict(os.environ, PRISME_CONFIG_DIR=str(self.profiles.parent))
        if extra_env:
            env.update(extra_env)
        process = subprocess.Popen(
            [
                str(DRIVER),
                str(HELPER),
                str(self.server.config),
                str(self.remote),
                str(self.root / "mounts"),
                key,
                mode,
                str(self.local),
            ],
            env=env,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        raw, errors = bytearray(), b""
        deadline = time.monotonic() + 45
        try:
            while time.monotonic() < deadline:
                ready = selector.select(0.2)
                if not ready and process.poll() is not None:
                    break
                if not ready:
                    continue
                chunk = os.read(process.stdout.fileno(), 65536)
                if not chunk:
                    break
                raw.extend(chunk)
                while b"\n" in raw:
                    line, _, raw = raw.partition(b"\n")
                    item = json.loads(line)
                    self.events.append(item)
                    if item["type"] == "auth":
                        proc = Path("/proc") / str(process.pid)
                        for name in ("cmdline", "environ"):
                            self.assertNotIn(
                                self.server.password.encode(),
                                (proc / name).read_bytes(),
                            )
                            self.assertNotIn(
                                self.server.passphrase.encode(),
                                (proc / name).read_bytes(),
                            )
                        if item["confirm"]:
                            self.assertIn("SHA256:", item["value"])
                            answer = "yes" if accept else "no"
                        else:
                            answer = (
                                None
                                if cancel
                                else "wrong-password"
                                if wrong
                                else self.server.passphrase
                                if self.server.mode == "key"
                                else self.server.password
                            )
                        process.stdin.write(
                            (
                                json.dumps(dict(answer=answer), ensure_ascii=False)
                                + "\n"
                            ).encode()
                        )
                        process.stdin.flush()
                    elif item["type"] == "drop_ready":
                        for transport in list(self.server.transports):
                            transport.close()
            code = process.wait(timeout=10)
            errors = process.stderr.read()
        finally:
            selector.close()
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            for stream in (process.stdin, process.stdout, process.stderr):
                stream.close()
        content = json.dumps(self.events, ensure_ascii=False).encode() + errors
        self.assertNotIn(self.server.password.encode(), content)
        self.assertNotIn(self.server.passphrase.encode(), content)
        self.assertFalse(
            any(item["type"] == "cleanup_error" for item in self.events), self.events
        )
        mounts = (self.root / "mounts").as_posix()
        self.assertNotIn(mounts, Path("/proc/self/mountinfo").read_text())
        self.assertTrue(self.profiles.is_file(), self.events)
        saved = json.loads(self.profiles.read_text())
        self.assertEqual(saved["schema"], 1)
        self.assertEqual(len(saved["profiles"]), 1)
        profile = saved["profiles"][0]
        self.assertEqual(
            set(profile), {"id", "host", "user", "port", "folder", "identity"}
        )
        self.assertEqual(
            (profile["host"], profile["user"], profile["port"], profile["folder"]),
            ("prisme-test", "odoo", "2222", str(self.remote)),
        )
        self.assertEqual(self.profiles.stat().st_mode & 0o777, 0o600)
        self.assertNotIn(self.server.password, self.profiles.read_text())
        self.assertNotIn(self.server.passphrase, self.profiles.read_text())
        self.assertEqual(self.outside.read_text(), "never changed")
        return code

    def assert_success(self, **kwargs):
        self.assertEqual(self.run_native(**kwargs), 0, self.events)
        self.assertTrue(
            any(item["type"] == "done" for item in self.events), self.events
        )
        self.assertIn("# saved by native C", (self.remote / "main.py").read_text())
        self.assertEqual(
            (self.remote / "upload-c.txt").read_text(),
            "uploaded through native guarded worker\n",
        )
        self.assertTrue((self.local / "main.py").is_file())
        self.assertFalse((self.remote / "created-c").exists())
        self.assertFalse((self.remote / "outside-link").is_symlink())
        self.assertEqual(set(self.server.authenticated_users), {"odoo"})
        self.assertEqual(self.server.client_port_file.read_text(), "2222")

    def test_password_explicit_odoo_and_port_crud_and_native_form(self):
        self.assert_success()

    def test_folder_search_remote_snapshots_f3_and_no_edits(self):
        first = self.remote / "a-search.txt"
        second = self.remote / "sub-search" / "b.txt"
        second.parent.mkdir()
        first.write_text("needle + needle\n")
        second.write_text("α\tneedle\n")
        self.assert_success(mode="search")
        self.assertEqual(
            sum(item["type"] == "folder_search_completed" for item in self.events),
            1,
            self.events,
        )
        self.assertEqual(first.read_text(), "needle + needle\n")
        self.assertEqual(second.read_text(), "α\tneedle\n")

    def test_xml_inline_from_remote_file_is_read_only_and_atomic(self):
        source = '<odoo><record id="view_order_form" model="ir.ui.view"><field name="name">sale.order.form</field><field name="model">sale.order</field><field name="arch" type="xml"><form><field name="partner_id"/></form></field></record><menuitem id="menu_sale"/></odoo>'
        path = self.remote / "sale" / "views" / "sale_views.xml"
        path.parent.mkdir(parents=True)
        path.write_text(source)
        self.assert_success(mode="xml")
        self.assertEqual(
            sum(item["type"] == "xml_inserted" for item in self.events), 2, self.events
        )
        self.assertEqual(
            sum(item["type"] == "directory_completed" for item in self.events), 2
        )
        self.assertEqual(path.read_text(), source)

    def test_remote_session_offline_restore_then_explicit_validation_and_external_conflict(
        self,
    ):
        self.assert_success(mode="session")
        self.assertTrue(
            any(item["type"] == "session_recovered" for item in self.events),
            self.events,
        )
        self.assertIn("external change", (self.remote / "main.py").read_text())
        for path in (self.local / "sessions").glob("*.state"):
            self.assertNotIn(self.server.password.encode(), path.read_bytes())
            self.assertNotIn(self.server.passphrase.encode(), path.read_bytes())

    def test_encrypted_private_key_path_spaces_and_passphrase(self):
        self.server.mode = "key"
        self.assert_success(key=str(self.server.private))
        self.assertTrue(
            any(
                "passphrase" in item["value"].lower()
                for item in self.events
                if item["type"] == "auth"
            )
        )

    def test_key_from_real_agent_without_secret_prompt(self):
        self.server.mode = "key"
        socket_path = self.root / "agent.sock"
        agent = subprocess.Popen(
            ["ssh-agent", "-D", "-a", str(socket_path)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )

        def close():
            agent.terminate()
            try:
                agent.wait(timeout=3)
            except subprocess.TimeoutExpired:
                agent.kill()
                agent.wait()
            agent.stderr.close()

        self.addCleanup(close)
        deadline = time.monotonic() + 3
        while not socket_path.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        key = self.root / "qa-agent-key"
        self.server.key.write_private_key_file(str(key))
        key.chmod(0o600)
        env = dict(os.environ, SSH_AUTH_SOCK=str(socket_path))
        result = subprocess.run(
            ["ssh-add", str(key)], env=env, capture_output=True, timeout=5
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assert_success(extra_env={"SSH_AUTH_SOCK": str(socket_path)})
        self.assertFalse(
            any(not item["confirm"] for item in self.events if item["type"] == "auth")
        )

    def test_unknown_host_refusal_preserves_nonsecret_profile(self):
        self.assertNotEqual(self.run_native(accept=False), 0)
        self.assertFalse((self.root / "known_hosts").exists())
        self.assertFalse(any(item["type"] == "ready" for item in self.events))

    def test_changed_host_key_is_not_overwritten(self):
        import paramiko

        key = paramiko.RSAKey.generate(2048)
        known = self.root / "known_hosts"
        original = (
            "[prisme-test]:2222 " + key.get_name() + " " + key.get_base64() + "\n"
        )
        known.write_text(original)
        self.assertNotEqual(self.run_native(), 0)
        self.assertEqual(known.read_text(), original)
        self.assertFalse(any(item["type"] == "ready" for item in self.events))
        self.assertTrue(
            any(
                "HOST IDENTIFICATION HAS CHANGED" in item["value"]
                for item in self.events
            )
        )

    def test_cancel_password_preserves_configuration(self):
        self.assertNotEqual(self.run_native(cancel=True), 0)
        self.assertFalse(any(item["type"] == "ready" for item in self.events))
        self.assertEqual((self.remote / "main.py").read_text(), "print('native SSH')\n")

    def test_wrong_password_preserves_configuration(self):
        self.assertNotEqual(self.run_native(wrong=True), 0)
        self.assertFalse(any(item["type"] == "ready" for item in self.events))

    def test_busy_mount_refuses_unmount_without_force(self):
        self.assert_success(mode="busy")
        self.assertTrue(
            any(item["type"] == "busy_protected" for item in self.events), self.events
        )

    def test_transport_loss_and_reconnection_preserve_dirty_buffer(self):
        self.assert_success(mode="reconnect")
        self.assertTrue(
            any(item["type"] == "reconnected" for item in self.events), self.events
        )
        self.assertIn("# kept draft", (self.remote / "main.py").read_text())
        self.assertIn("# kept draft", (self.local / "salvage.py").read_text())

    def test_missing_identity_still_remembers_configuration(self):
        missing = str(self.root / "unavailable key")
        self.assertNotEqual(self.run_native(key=missing), 0)
        self.assertEqual(
            json.loads(self.profiles.read_text())["profiles"][0]["identity"], missing
        )
        self.assertFalse(self.server.authenticated_users)


if __name__ == "__main__":
    unittest.main(verbosity=2)
