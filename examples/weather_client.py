"""Example out-of-process provider for Search Menu Extensions.

Type "weather <city>" in the new Windows search box. Uses wttr.in (no API key).
Run:  python weather_client.py
"""
import json
import threading
import time
import urllib.parse
import urllib.request
import ctypes
import os


def pipe_name():
    # One pipe per Windows session: \\.\pipe\WindhawkSearchExtensions-<session id>
    session = ctypes.c_ulong()
    ctypes.windll.kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(session))
    return r'\\.\pipe\WindhawkSearchExtensions-%d' % session.value


PIPE = pipe_name()


class Client:
    def __init__(self):
        # Retry until the composer (and the mod inside it) is up.
        while True:
            try:
                self.f = open(PIPE, 'r+b', buffering=0)
                break
            except OSError:
                time.sleep(2)
        self.lock = threading.Lock()

    def send(self, msg):
        data = (json.dumps(msg, ensure_ascii=False) + '\n').encode('utf-8')
        with self.lock:
            self.f.write(data)

    def messages(self):
        buf = b''
        while True:
            chunk = self.f.read(65536)
            if not chunk:
                return
            buf += chunk
            while b'\n' in buf:
                line, buf = buf.split(b'\n', 1)
                if line.strip():
                    yield json.loads(line)


GLYPHS = {'sun': '\uE706', 'cloud': '\uE753', 'rain': '\uE9E2', 'snow': '\uE9C8', 'thunder': '\uE9C6'}


def glyph_for(desc):
    d = desc.lower()
    for k, g in (('thunder', 'thunder'), ('snow', 'snow'), ('rain', 'rain'), ('drizzle', 'rain'),
                 ('shower', 'rain'), ('cloud', 'cloud'), ('overcast', 'cloud'), ('mist', 'cloud'), ('fog', 'cloud')):
        if k in d:
            return GLYPHS[g]
    return GLYPHS['sun']


def fetch(city):
    url = 'https://wttr.in/%s?format=j1' % urllib.parse.quote(city)
    req = urllib.request.Request(url, headers={'User-Agent': 'search-menu-extensions-example'})
    with urllib.request.urlopen(req, timeout=4) as r:
        j = json.load(r)
    cur = j['current_condition'][0]
    area = j['nearest_area'][0]
    name = area['areaName'][0]['value'] + ', ' + area['country'][0]['value']
    return name, cur


def answer(c, q, city):
    try:
        name, cur = fetch(city)
    except Exception as e:  # noqa: BLE001
        c.send({'op': 'results', 'queryId': q, 'done': True,
                'items': [{'id': 'err', 'title': 'Weather unavailable', 'subtitle': str(e)[:120],
                           'icon': {'glyph': '\uE783'}, 'action': {'type': 'none'}}]})
        return
    desc = cur['weatherDesc'][0]['value']
    title = '%s: %s°C, %s' % (name, cur['temp_C'], desc.lower())
    item = {
        'id': 'now',
        'title': title,
        'subtitle': 'Feels like %s°C · humidity %s%% · wind %s km/h' % (cur['FeelsLikeC'], cur['humidity'],
                                                                    cur['windspeedKmph']),
        'icon': {'glyph': glyph_for(desc)},
        'score': 1.0,
        'tags': [{'text': 'wttr.in'}],
        'action': {'type': 'uri', 'uri': 'https://wttr.in/' + urllib.parse.quote(city)},
        'card': {
            'template': {
                'type': 'AdaptiveCard', 'version': '1.5',
                'body': [
                    {'type': 'TextBlock', 'text': '${place}', 'weight': 'Bolder', 'size': 'Medium'},
                    {'type': 'TextBlock', 'text': '${temp}°C', 'size': 'ExtraLarge', 'spacing': 'None'},
                    {'type': 'TextBlock', 'text': '${desc}', 'isSubtle': True, 'spacing': 'None'},
                ],
            },
            'data': {'place': name, 'temp': cur['temp_C'], 'desc': desc},
        },
    }
    c.send({'op': 'results', 'queryId': q, 'items': [item], 'done': True})


def main():
    while True:
        try:
            serve()
        except OSError:
            pass
        time.sleep(2)  # composer restarted or library reloaded; reconnect


def serve():
    c = Client()
    c.send({'op': 'register', 'provider': {'id': 'weather', 'name': 'Weather (wttr.in)', 'glyph': '\uE706'}})
    for m in c.messages():
        op = m.get('op')
        if op == 'query':
            text = m['text'].strip()
            if text.lower().startswith('weather') and len(text) > 8:
                # Answer inline: a synchronous pipe handle can't write while another thread is
                # blocked reading it (Windows serializes I/O on non-overlapped handles).
                answer(c, m['queryId'], text[8:].strip())
            else:
                c.send({'op': 'results', 'queryId': m['queryId'], 'items': [], 'done': True})
        elif op in ('registered', 'error'):
            print(m)


if __name__ == '__main__':
    main()
