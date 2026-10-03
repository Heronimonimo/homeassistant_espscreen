"""Wake an open firmware preview when one of its HA entities changes.

Only invalidations cross this stream. The preview still reads the normal firmware
packets, never raw HA attributes, through its existing endpoint.
"""
import asyncio
import re

from aiohttp import web


# A screen's tiles plus the entity items of its pages' top bars (firmware 0.34.0+): 256 tiles and twelve items on each of
# 16 pages on a P4 board (448), 128 tiles and twelve on each of 24 pages on an S3 board (416). core.STORE_MAX_* allows
# more, which no board states today.
MOST_ENTITIES = 512


class Changes:
    def __init__(self):
        self.listeners = {}

    def notify(self, entity=None):
        for wake, entities in self.listeners.items():
            if entity is None or entity in entities:
                wake.set()


async def stream(changes, request):
    entities = request.query.getall('entity', [])
    if (not 1 <= len(entities) <= MOST_ENTITIES or
            any(len(entity) > 120 or not re.fullmatch(r'[a-z0-9_]+\.[a-z0-9_]+', entity) for entity in entities)):
        raise ValueError('Invalid preview entity subscription.')
    response = web.StreamResponse(headers={'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store',
                                           'X-Accel-Buffering': 'no', 'X-Content-Type-Options': 'nosniff'})
    wake = asyncio.Event()
    changes.listeners[wake] = frozenset(entities)
    wake.set()  # Also refresh after reconnection; events may have been missed.
    try:
        await response.prepare(request)
        while True:
            try:
                await asyncio.wait_for(wake.wait(), 15)
            except TimeoutError:
                await response.write(b': keepalive\n\n')
            else:
                wake.clear()
                await response.write(b'data: {}\n\n')
    except (ConnectionResetError, asyncio.CancelledError):
        pass
    finally:
        changes.listeners.pop(wake, None)
    return response
