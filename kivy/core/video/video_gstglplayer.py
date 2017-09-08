'''
Video GstGLplayer
===============

.. versionadded:: 1.10.2

Implementation of a VideoBase with Kivy :class:`~kivy.lib.gstglplayer.GstGLPlayer`
Uses Gstreamer 1.0's OpenGL support for smooth video playback on supported platforms.
'''

from kivy.clock import Clock
from kivy.compat import PY2
from kivy.core.video import VideoBase
from kivy.graphics.texture import Texture
from kivy.graphics.opengl import GL_TEXTURE_2D
from kivy.lib.gstglplayer import GstGLPlayer, get_gst_version
from kivy.logger import Logger
from threading import Lock
from functools import partial
from os.path import realpath
from weakref import ref

if PY2:
    from urllib import pathname2url
else:
    from urllib.request import pathname2url

Logger.info('VideoGstGLplayer: Using Gstreamer {}'.format(
    '.'.join(map(str, get_gst_version()))))


def _on_gstplayer_message(mtype, message):
    if mtype == 'error':
        Logger.error('VideoGstGLplayer: {}'.format(message))
    elif mtype == 'warning':
        Logger.warning('VideoGstGLplayer: {}'.format(message))
    elif mtype == 'info':
        Logger.info('VideoGstGLplayer: {}'.format(message))


class VideoGstGLplayer(VideoBase):

    def __init__(self, **kwargs):
        self.player = None
        self._next_texture = None
        self._on_load_called = False
        super(VideoGstGLplayer, self).__init__(**kwargs)

    def _on_gst_eos_sync(self):
        Clock.schedule_once(self._do_eos, 0)

    def load(self):
        Logger.debug('VideoGstGLplayer: Load <{}>'.format(self._filename))
        uri = self._get_uri()
        wk_self = ref(self)
        self.player = GstGLPlayer(uri, self._update_texture,
                                self._on_gst_eos_sync, _on_gstplayer_message)
        self.player.load()

    def unload(self):
        if self.player:
            self.player.unload()
            self.player = None

    def stop(self):
        super(VideoGstGLplayer, self).stop()
        self.player.stop()

    def pause(self):
        super(VideoGstGLplayer, self).pause()
        self.player.pause()

    def play(self):
        super(VideoGstGLplayer, self).play()
        self.player.set_volume(self.volume)
        self.player.play()

    def seek(self, percent):
        self.player.seek(percent)

    def _get_position(self):
        return self.player.get_position()

    def _get_duration(self):
        return self.player.get_duration()

    def _set_volume(self, value):
        self._volume = value
        if self.player:
            self.player.set_volume(self._volume)

    def _update(self, dt):
        if self._texture:
            # There's probably a better way to do this, TODO
            if not self._on_load_called:
                self.dispatch('on_load')
                self._on_load_called = True

            self.dispatch('on_frame')

    def _update_texture(self, width, height, tex_id):
        self._texture = texture = Texture(
            width,
            height,
            GL_TEXTURE_2D,
            texid=tex_id,
        )
        texture.flip_vertical()

        return texture

    def _get_uri(self):
        uri = self.filename
        if not uri:
            return
        if '://' not in uri:
            uri = 'file:' + pathname2url(realpath(uri))
        return uri
