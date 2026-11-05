"""Serveur SSH de QA éphémère sur socket Unix, jamais de port/service TCP."""

from pathlib import Path
import shlex
import socket
import subprocess
import sys
import threading
import paramiko


class PipeSFTP(paramiko.SubsystemHandler):
    def start_subsystem(self, name, transport, channel):
        process = subprocess.Popen(
            ["/usr/lib/openssh/sftp-server"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )

        def output():
            try:
                while chunk := process.stdout.read1(32768):
                    channel.sendall(chunk)
            except (OSError, EOFError):
                pass
            finally:
                channel.close()

        thread = threading.Thread(target=output, daemon=True)
        thread.start()
        try:
            while chunk := channel.recv(32768):
                process.stdin.write(chunk)
                process.stdin.flush()
        except (OSError, EOFError):
            pass
        finally:
            process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            process.stdin.close()
            thread.join(2)
            process.stdout.close()


class UnixSSHServer:
    def __init__(self, directory, mode="password", hostkey=None):
        self.directory = Path(directory)
        self.path = self.directory / "ssh.sock"
        self.mode = mode
        self.expected_user = "test"
        self.authenticated_users = []
        self.client_port_file = self.directory / "client-port"
        self.password = "test-password-é"
        self.passphrase = "test-key-passphrase"
        self.key = paramiko.RSAKey.generate(2048)
        self.hostkey = hostkey or paramiko.RSAKey.generate(2048)
        self.private = self.directory / "identity with spaces"
        self.key.write_private_key_file(str(self.private), password=self.passphrase)
        self.private.chmod(0o600)
        Path(str(self.private) + ".pub").write_text(
            self.key.get_name() + " " + self.key.get_base64() + "\n"
        )
        self.stop = threading.Event()
        self.transports = []
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.bind(str(self.path))
        self.socket.listen(2)
        self.socket.settimeout(0.2)
        server = self

        class Authentication(paramiko.ServerInterface):
            def get_allowed_auths(self, username):
                return "publickey" if server.mode == "key" else "password"

            def check_auth_password(self, username, password):
                if (
                    server.mode == "password"
                    and username == server.expected_user
                    and password == server.password
                ):
                    server.authenticated_users.append(username)
                    return paramiko.AUTH_SUCCESSFUL
                return paramiko.AUTH_FAILED

            def check_auth_publickey(self, username, key):
                if (
                    server.mode == "key"
                    and username == server.expected_user
                    and key == server.key
                ):
                    server.authenticated_users.append(username)
                    return paramiko.AUTH_SUCCESSFUL
                return paramiko.AUTH_FAILED

            def check_channel_request(self, kind, chanid):
                return (
                    paramiko.OPEN_SUCCEEDED
                    if kind == "session"
                    else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED
                )

        self.authentication = Authentication
        self.thread = threading.Thread(target=self._accept, daemon=True)
        self.thread.start()
        proxy = self.directory / "proxy.py"
        proxy.write_text("""import os,selectors,socket,sys
from pathlib import Path
Path(sys.argv[2]).write_text(sys.argv[3])
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);s.connect(sys.argv[1])
sel=selectors.DefaultSelector();sel.register(0,selectors.EVENT_READ);sel.register(s,selectors.EVENT_READ)
running=True
while running:
 for key,_ in sel.select():
  if key.fileobj==0:
   data=os.read(0,32768)
   if not data:running=False;break
   s.sendall(data)
  else:
   data=s.recv(32768)
   if not data:running=False;break
   os.write(1,data)
s.close()
""")
        self.config = self.directory / "config"
        self.config.write_text(f'''Host prisme-test
  HostName prisme-test
  User test
  Port 22
  ProxyCommand {shlex.quote(sys.executable)} -I -B {shlex.quote(str(proxy))} {shlex.quote(str(self.path))} {shlex.quote(str(self.client_port_file))} %p
  UserKnownHostsFile "{self.directory / "known_hosts"}"
  GlobalKnownHostsFile /dev/null
  IdentitiesOnly yes
  IdentityFile "{self.private}"
  KbdInteractiveAuthentication no
  LogLevel INFO
''')
        self.config.chmod(0o600)

    def _accept(self):
        while not self.stop.is_set():
            try:
                connection, _ = self.socket.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            def session(sock):
                transport = paramiko.Transport(sock)
                self.transports.append(transport)
                transport.add_server_key(self.hostkey)
                transport.set_subsystem_handler("sftp", PipeSFTP)
                try:
                    transport.start_server(server=self.authentication())
                    while transport.is_active() and not self.stop.wait(0.05):
                        pass
                except (OSError, EOFError, paramiko.SSHException):
                    pass
                finally:
                    transport.close()

            threading.Thread(target=session, args=(connection,), daemon=True).start()

    def close(self):
        self.stop.set()
        self.socket.close()
        for transport in self.transports:
            transport.close()
        self.thread.join(2)
