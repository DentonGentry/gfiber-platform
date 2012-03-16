#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
#
"""A simple app to let users upload files, then other users download them."""

__author__ = 'apenwarr@google.com (Avery Pennarun)'

import logging
import os
import re
import urllib
import zlib
from google.appengine.api import users
from google.appengine.ext import blobstore
from google.appengine.ext import db
from google.appengine.ext import webapp
from google.appengine.ext.webapp import blobstore_handlers
from google.appengine.ext.webapp import template


_path = os.path.dirname(__file__)


def _SearchWords(s):
  return re.sub(r'[^\w\s]+', ' ', s).lower().split()


STOPWORDS = ['the', 'a', 'in']


def _SkipStopWords(words):
  regex = re.compile(r'\d*$')
  for word in words:
    if word not in STOPWORDS and not regex.match(word):
      yield word


class File(db.Model):
  """Represents an uploaded file."""
  name = db.StringProperty()
  blobid = blobstore.BlobReferenceProperty()
  create_time = db.DateTimeProperty(auto_now=True)
  meta = db.StringListProperty()

  @property
  def key_name(self):
    return self.key().name

  @property
  def nice_create_time(self):
    return self.create_time.strftime('%Y-%m-%d %H:%M:%S UTC')

  def __repr__(self):
    return 'File(%s)' % self.name

  @classmethod
  def New(cls, name, blobid, meta=None):
    meta = [('%s=%s' % i) for i in (meta or [])]
    return cls(name=name, blobid=blobid, meta=meta)


class Words(db.Model):
  file = db.ReferenceProperty(File)
  words = db.StringListProperty()
  create_time = db.DateTimeProperty(auto_now=True)


def Render(filename, **kwargs):
  """A wrapper for template.render that handles some boilerplate.

  Args:
    filename: the basename (not the full path) of the template file.
    **kwargs: additional contents for the rendering dictionary.
  Returns:
    The rendered template text.
  """
  return template.render(os.path.join(_path, '.', filename), kwargs)


class Handler(webapp.RequestHandler):
  """A wrapper for webapp.RequestHandler providing some helper functions."""
  Redirect = webapp.RequestHandler.redirect

  def Write(self, *args):
    self.response.out.write(*args)

  def Render(self, filename, **kwargs):
    self.Write(Render(filename, **kwargs))

  def ValidateUser(self):
    user = users.get_current_user()
    if not user or not user.email().endswith('@google.com'):
      raise Exception('invalid user')


class Main(Handler):
  """Returns a list of available files."""

  #pylint: disable-msg=C6409
  def get(self):
    """HTTP GET handler."""
    self.ValidateUser()
    query = self.request.get('q')
    words = _SearchWords(query)
    if words:
      qlist = []
      # start all the datastore queries at once, so they run in parallel.
      # They don't become synchronous until we actually start iterating
      # through them.
      for word in words:
        qlist.append(list(db.Query(Words, keys_only=True)
                          .order('-create_time')
                          .filter('words', word)
                          .run()))
      sets = [set(w.parent() for w in q) for q in qlist]
      keys = set.intersection(*sets)
      files = File.get(keys)
      files.sort(key=lambda f: f.create_time, reverse=True)
    else:
      files = File.all().order('-create_time')
    self.Render('files.djt', files=files, query=query)


BUFSIZE = 1048576


def _DecompressBlob(blobkey):
  blob = blobstore.BlobReader(blobkey, buffer_size=BUFSIZE)
  decomp = zlib.decompressobj()
  while 1:
    data = blob.read(BUFSIZE)
    if not data:
      break
    yield decomp.decompress(data, BUFSIZE)
    while decomp.unconsumed_tail:
      yield decomp.decompress(decomp.unconsumed_tail, BUFSIZE)
  yield decomp.flush(BUFSIZE)
  while decomp.unconsumed_tail:
    yield decomp.flush(BUFSIZE)


def _AllArgs(req, keys):
  query = []
  if keys:
    for key in keys:
      for val in req.get_all(key):
        query.append((key, val))
  return query


class Upload(Handler, blobstore_handlers.BlobstoreUploadHandler):
  """Allows the user to upload a file."""

  #pylint: disable-msg=W0221
  def get(self, filename):
    """HTTP GET handler.  Returns an URL that can be used for uploads."""
    path = '/upload/%s' % filename
    query = _AllArgs(self.request, self.request.arguments())
    if query:
      for key in dict(query).iterkeys():
        query.append(('_', key))
      path += '?' + urllib.urlencode(query)
    url = blobstore.create_upload_url(path)
    self.Write(url)

  #pylint: disable-msg=C6409
  #pylint: disable-msg=W0221
  def post(self, filename):
    """HTTP POST handler."""
    uploads = list(self.get_uploads('file'))
    if len(uploads) != 1:
      logging.error('Exactly one attachment expected; got %d.',
                    len(uploads))
      self.error(500)
    meta = _AllArgs(self.request, self.request.get_all('_'))
    f = File.New(name=filename, blobid=uploads[0].key(), meta=meta)
    f.put()

    words = set()
    for bit in _SearchWords(filename):
      words.add(bit)
    for key, value in meta:
      for bit in _SearchWords(key) + _SearchWords(value):
        words.add(bit)
    for chunk in _DecompressBlob(f.blobid.key()):
      bits = _SkipStopWords(_SearchWords(chunk))
      for bit in bits:
        words.add(bit)
    words = list(words)
    WORDS_PER_ROW = 100
    for i in xrange(0, len(words), WORDS_PER_ROW):
      subset = words[i:i+WORDS_PER_ROW]
      w = Words(parent=f, file=f.key(), words=subset)
      w.put()
      break  # for now, allow only one pass, to not exceed appengine quota :(

    self.Redirect('/')


class Download(Handler):
  """Retrieves a given file from the blobstore."""

  #pylint: disable-msg=C6409
  #pylint: disable-msg=W0221
  def get(self, key):
    """HTTP GET handler."""
    self.ValidateUser()
    f = File.get(key)
    if not f:
      return self.error(404)

    self.response.headers.add_header('Content-Type', 'text/plain')

    # AppEngine is annoying and won't just let us serve pre-encoded zlib
    # encoded files.  So let's decompress it and let appengine recompress
    # it if the client supports it.
    #pylint: disable-msg=E1103
    for data in _DecompressBlob(f.blobid.key()):
      self.Write(data)

wsgi_app = webapp.WSGIApplication([
    ('/', Main),
    ('/upload/(.+)', Upload),
    ('/([^/]+)/.+', Download),
], debug=True)
