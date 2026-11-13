import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.client import HTTPConnection
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'server'))
import server as npc


class Tests(unittest.TestCase):
    def setUp(self):
        self.service = npc.Service(npc.Config())
        self.server = npc.Server(('127.0.0.1', 0), self.service)
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={'poll_interval': .01}, daemon=True)
        self.thread.start()
        self.port = self.server.server_port

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(1)

    def req(self, obj=None, path='/chat', method='POST', headers=None, raw=None):
        conn = HTTPConnection('127.0.0.1', self.port, timeout=2)
        body = raw if raw is not None else json.dumps(obj, ensure_ascii=False).encode() if obj is not None else None
        conn.request(method, path, body, headers or {'Content-Type': 'application/json'})
        response = conn.getresponse()
        result = response.status, response.read().decode()
        conn.close()
        return result

    def data(self, npc_id='s123.p45', msg='Olá, você é real?', **kw):
        return dict(npc_id=npc_id, message=msg, **kw)

    def test_health(self):
        status, text = self.req(method='GET', path='/health')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(text), {'ok': True, 'mode': 'mock'})

    def test_mock_simulation(self):
        code, text = self.req(self.data(msg='Você vive numa simulação?'))
        self.assertEqual(code, 200)
        self.assertIn('simulação', text)

    def test_dynamic_identifiers(self):
        for name in ('s9a88.p789', 'ped-2058725', 'qualquer_NPC:42'):
            self.assertEqual(self.req(self.data(name))[0], 200)
            self.assertIn((name, 'default'), self.service.history)

    def test_utf8(self):
        text = 'Olá! Ação, coração, Ç, Ã, ê, 😀'
        code, reply = self.req(self.data(msg=text))
        self.assertEqual(code, 200)
        self.assertIn(text, reply)

    def test_isolation(self):
        self.req(self.data('A', 'segredo A'))
        self.req(self.data('B', 'segredo B'))
        self.assertNotIn('segredo A', str(self.service.history[('B', 'default')]))
        self.assertNotIn('segredo B', str(self.service.history[('A', 'default')]))

    def test_conversation_namespace(self):
        self.req(self.data('A', 'antes', conversation_id='c1'))
        self.req(self.data('A', 'depois', conversation_id='c2'))
        self.assertNotIn('antes', str(self.service.history[('A', 'c2')]))

    def test_mock_memory(self):
        self.req(self.data(msg='meu carro é azul'))
        code, text = self.req(self.data(msg='Você lembra?'))
        self.assertEqual(code, 200)
        self.assertIn('meu carro é azul', text)

    def test_reset(self):
        self.req(self.data())
        self.assertEqual(self.req({'npc_id': 's123.p45'}, path='/reset')[0], 200)
        self.assertNotIn(('s123.p45', 'default'), self.service.history)

    def test_bad_ids(self):
        for value in ('', '../escape', 'x' * 161, 42, None, 'á'):
            self.assertEqual(self.req(self.data(value))[0], 400)

    def test_invalid_body(self):
        for raw in (b'[]', b'null', b'{bad', b'\xff', b'{"npc_id":"a","message":"\\ud800"}'):
            self.assertEqual(self.req(raw=raw)[0], 400)
        self.assertEqual(self.req(raw=b'x' * 8193)[0], 413)

    def test_message_bound(self):
        for text in ('', '   ', 'á' * 601, '\x00'):
            self.assertEqual(self.req(self.data(msg=text))[0], 400)
        self.assertEqual(self.req(self.data(msg='á' * 600))[0], 200)

    def test_context_validation(self):
        self.assertEqual(self.req(self.data(context={'model': True}))[0], 400)
        self.assertEqual(self.req(self.data(context={'hour': 24}))[0], 400)
        self.assertEqual(self.req(self.data(context=[]))[0], 400)
        self.assertEqual(self.req(self.data(context={'model': 105, 'hour': 23, 'in_vehicle': True}))[0], 200)

    def test_personas(self):
        for context, expected in (({'model': 105}, 'Families'), ({'model': 102}, 'Ballas'),
                                  ({'ped_type': 6}, 'Policial'), ({'model': 262}, 'Pedestre'), ({'observed_taxi_driver': True}, 'Taxista'), ({}, 'Pedestre')):
            self.assertIn(expected, npc.persona(context)[0])

    def test_auth(self):
        self.service.config.token = 'senha-local'
        self.assertEqual(self.req(self.data())[0], 401)
        headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer senha-local'}
        self.assertEqual(self.req(self.data(), headers=headers)[0], 200)
        self.assertEqual(self.req(method='GET', path='/health')[0], 401)

    def test_content_type(self):
        self.assertEqual(self.req(self.data(), headers={'Content-Type': 'text/plain'})[0], 415)

    def test_history_bound(self):
        self.service.config.history_limit = 4
        for i in range(5):
            self.req(self.data(msg=str(i)))
        messages = self.service.history[('s123.p45', 'default')]['messages']
        self.assertEqual(len(messages), 4)
        self.assertEqual(messages[0]['content'], '3')

    def test_lru_memory_bound(self):
        self.service.config.max_conversations = 2
        for i in range(5):
            self.req(self.data(str(i)))
        self.assertEqual(len(self.service.history), 2)
        self.assertIn(('4', 'default'), self.service.history)

    def test_persistence(self):
        with tempfile.TemporaryDirectory() as directory:
            path = str(Path(directory) / 'memory.json')
            self.service.config.memory_file = path
            self.req(self.data(msg='ação'))
            loaded = npc.Service(npc.Config(memory_file=path))
            self.assertIn('ação', str(loaded.history))
            self.assertEqual(os.stat(path).st_mode & 0o777, 0o600)
            self.assertNotIn('api_key', Path(path).read_text())

    def test_persistence_byte_bound(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(npc, 'MAX_MEMORY_FILE', 1500):
            path = str(Path(directory) / 'memory.json')
            self.service.config.memory_file = path
            for i in range(10):
                self.req(self.data(str(i), 'ação ' * 25))
            self.assertLessEqual(Path(path).stat().st_size, 1500)
            loaded = npc.Service(npc.Config(memory_file=path))
            self.assertIn(('9', 'default'), loaded.history)
            self.assertLess(len(loaded.history), len(self.service.history))

    def test_reset_discards_late_response(self):
        started, release = threading.Event(), threading.Event()
        def generator(context, messages):
            started.set()
            release.wait(2)
            return 'ATRASADA'
        self.service.generator = generator
        replies = []
        thread = threading.Thread(target=lambda: replies.append(self.req(self.data())))
        thread.start()
        self.assertTrue(started.wait(1))
        self.assertEqual(self.req({'npc_id': 's123.p45'}, path='/reset')[0], 200)
        release.set()
        thread.join(2)
        self.assertEqual(replies[0][0], 409)
        self.assertNotIn(('s123.p45', 'default'), self.service.history)

    def test_concurrency_limit(self):
        started, release = threading.Event(), threading.Event()
        self.service.config.max_inflight = 1
        def generator(context, messages):
            started.set()
            release.wait(2)
            return 'ok'
        self.service.generator = generator
        thread = threading.Thread(target=lambda: self.req(self.data('A')))
        thread.start()
        self.assertTrue(started.wait(1))
        self.assertEqual(self.req(self.data('B'))[0], 429)
        self.assertEqual(self.req(self.data('A'))[0], 429)
        release.set()
        thread.join(2)

    def test_provider_failure_is_not_saved(self):
        def generator(context, messages):
            raise RuntimeError('API offline')
        self.service.generator = generator
        self.assertEqual(self.req(self.data())[0], 502)
        self.assertFalse(self.service.history[('s123.p45', 'default')]['messages'])
        self.assertFalse(self.service.active)

    def test_responses_api_contract(self):
        class Response:
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def read(self, limit):
                return json.dumps({'output': [{'type': 'message', 'content': [{'type': 'output_text', 'text': 'Olá, CJ!'}]}]}).encode()
        def fake(req, timeout):
            self.assertEqual(req.full_url, 'https://api.openai.com/v1/responses')
            body = json.loads(req.data)
            self.assertEqual(body['model'], 'modelo-configurável')
            self.assertFalse(body['store'])
            self.assertTrue(body['text']['format']['strict'])
            self.assertEqual(body['text']['format']['type'],'json_schema')
            self.assertIn('surrender_money',body['text']['format']['schema']['properties']['action']['enum'])
            self.assertEqual(body['input'][-1]['content'], 'Oi!')
            self.assertEqual(timeout, 20)
            return Response()
        with patch.object(npc, 'urlopen', side_effect=fake):
            result = npc.generate_openai(npc.Config(model='modelo-configurável', api_key='fake-test-key'), {}, [{'role': 'user', 'content': 'Oi!'}])
        self.assertEqual(result, 'Olá, CJ!')


class NativeHTTP(unittest.TestCase):
    setUp = Tests.setUp
    tearDown = Tests.tearDown

    def run_client(self, port=None, timeout=1000, token='', cancel=False):
        binary = Path(__file__).parents[1] / 'build/test_http'
        if not binary.exists():
            self.fail('Execute ./tools/test.sh para compilar o cliente nativo')
        cmd = [str(binary), str(self.port if port is None else port), str(timeout), token]
        if cancel: cmd.append('cancel')
        return subprocess.run(cmd, capture_output=True, text=True, timeout=3)

    def test_native_utf8_dynamic_and_auth(self):
        self.service.config.token = 'local-token'
        result = self.run_client(token='local-token')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('simulação', result.stdout)
        self.assertIn(('session-123.ped-567', 'c1'), self.service.history)
        self.assertEqual(self.run_client().returncode, 1)

    def test_offline(self):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
            result = self.run_client(port=port)
        self.assertEqual(result.returncode, 1)
        self.assertIn('offline', result.stdout)

    def test_timeout(self):
        self.service.generator = lambda *args: (time.sleep(.3) or 'tarde')
        started = time.monotonic()
        result = self.run_client(timeout=80)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Timeout', result.stdout)
        self.assertLess(time.monotonic() - started, .8)

    def test_cancel_real_socket(self):
        self.service.generator = lambda *args: (time.sleep(.3) or 'tarde')
        started = time.monotonic()
        result = self.run_client(cancel=True)
        self.assertEqual(result.returncode, 1)
        self.assertLess(time.monotonic() - started, .6)

    def raw_server(self, response):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen()
        def serve():
            with listener:
                conn, _ = listener.accept()
                with conn:
                    conn.recv(8192)
                    conn.sendall(response)
        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        result = self.run_client(port=listener.getsockname()[1])
        thread.join(1)
        return result

    def test_truncated_http(self):
        self.assertEqual(self.raw_server(b'HTTP/1.0 200 OK\r\nContent-Length: 10\r\n\r\nabc').returncode, 1)

    def test_large_http(self):
        self.assertEqual(self.raw_server(b'HTTP/1.0 200 OK\r\nContent-Length: 99999\r\n\r\n').returncode, 1)

    def test_invalid_utf8_reply(self):
        self.assertEqual(self.raw_server(b'HTTP/1.0 200 OK\r\nContent-Length: 1\r\n\r\n\xff').returncode, 1)

    def test_duplicate_length(self):
        self.assertEqual(self.raw_server(b'HTTP/1.0 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx').returncode, 1)

    def test_status_error(self):
        result = self.raw_server('HTTP/1.0 503 Error\r\nContent-Length: 5\r\n\r\nerro!'.encode())
        self.assertEqual(result.returncode, 1)
        self.assertIn('503', result.stdout)

if __name__ == '__main__':
    unittest.main()
