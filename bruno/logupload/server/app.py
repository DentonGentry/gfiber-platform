#!/usr/bin/python
# Copyright 2012 Google Inc. All Rights Reserved.
#
# gpylint function naming pretty much sucks for appengine apps; disable the
# offending complaints:
#gpylint: disable-msg=C6409
#
"""A simple app to let users upload files, then other users download them.

This service is used by the upload-logs script in Google Fiber's set top box.
"""

__author__ = 'apenwarr@google.com (Avery Pennarun)'

import datetime
import logging
import os
import re
import time
import urllib
import zlib
from google.appengine.api import memcache
from google.appengine.api import taskqueue
from google.appengine.api import users
from google.appengine.ext import blobstore
from google.appengine.ext import ndb
from google.appengine.ext import webapp
from google.appengine.ext.webapp import blobstore_handlers
from google.appengine.ext.webapp import template


MAX_UNCOMPRESSED_BYTES = 5*1024*1024  # max bytes in a single download
MAX_MEM_PER_DOWNLOAD = 10*1024*1024    # max memory for buffering a download
MAX_PARALLEL_BLOBS = 64                # max blobstore requests at once

BUFSIZE = min(blobstore.MAX_BLOB_FETCH_SIZE,
              MAX_MEM_PER_DOWNLOAD / MAX_PARALLEL_BLOBS)
PARALLEL_BLOBS = min(MAX_MEM_PER_DOWNLOAD / BUFSIZE, MAX_PARALLEL_BLOBS)


_path = os.path.dirname(__file__)
_futures_queue = []
_memcache = memcache.Client()


def sync():
  while _futures_queue:
    f = _futures_queue.pop()
    f.get_result()  # may throw exception


class _Model(ndb.Model):
  def put_async(self):
    """put() in the background.  Call sync() to wait for all pending puts."""
    f = ndb.Model.put_async(self)
    _futures_queue.append(f)
    return f

  @property
  def key_name(self):
    """Property for the primary key, which you can alias to a member."""
    return self.key.id


class Machine(_Model):
  """Represents a machine that uploads files.

  Eventually we can have user-defined labels etc. for each machine.  For now,
  it's mostly useful as an efficient way of getting a list of distinct
  machines.
  """
  keymeta = _Model.key_name  # key=value string to match in File list
  modified_time = ndb.DateTimeProperty(auto_now=True)

  def __repr__(self):
    return 'Machine(%s)' % self.keymeta

  def files(self):
    return File.query().filter(File.meta == self.keymeta())

  @property
  def nice_modified_time(self):
    return self.modified_time.strftime('%Y-%m-%d %H:%M:%S UTC')

  def most_recent_meta(self):
    f = self.files().order(-File.create_time).get()
    meta = f and f.meta or []
    _memcache.set('meta-%s' % self.keymeta, meta)
    return meta


class File(_Model):
  """Represents an uploaded file."""
  name = ndb.StringProperty()  # filename
  blobid = ndb.BlobKeyProperty()  # pointer to gzip'd file content
  size = ndb.IntegerProperty()  # uncompressed file size
  create_time = ndb.DateTimeProperty(auto_now_add=True)  # when it was uploaded
  meta = ndb.StringProperty(repeated=True)  # metadata: key=value strings

  # We split keys and values here so we index the database by key, and thus
  # search for all rows with key='panic', ordered by date, etc.
  flag_keys = ndb.StringProperty(repeated=True)    # list of flag names
  flag_values = ndb.StringProperty(repeated=True)  # list of flag values

  def _get_flags(self):
    return zip(self.flag_keys, self.flag_values)

  def _set_flags(self, flags):
    self.flag_keys = [i[0] for i in flags]
    self.flag_values = [i[1] for i in flags]

  flags = property(fget=_get_flags, fset=_set_flags)

  @property
  def nice_create_time(self):
    return self.create_time.strftime('%Y-%m-%d %H:%M:%S UTC')

  @property
  def create_time_code(self):
    return self.create_time.strftime('%Y%m%d-%H%M%S')

  def __repr__(self):
    return 'File(%s)' % self.name

  @classmethod
  def New(cls, name, blobid, size, meta=None, flags=None):
    meta = [('%s=%s' % i) for i in (meta or [])]
    flag_keys = [i[0] for i in flags]
    flag_values = [i[1] for i in flags]
    return cls(name=name, blobid=blobid, size=size, meta=meta,
               flag_keys=flag_keys,
               flag_values=flag_values)

  def machine_key(self):
    """Return a key=value string that uniquely identifies the Machine."""
    for kv in self.meta:
      if kv.startswith('hw='):
        return kv
    for kv in self.meta:
      if kv.startswith('serial='):
        return kv
    if self.meta:
      return self.meta[0]
    return 'nometa'

  def machine(self):
    """Return a Machine object that owns this file; may create one."""
    for machine in ndb.get_multi(ndb.Key(Machine, i) for i in self.meta):
      if machine:
        return machine
    # still here? need to create it then.
    machine = Machine(id=self.machine_key())
    machine.put_async()
    return machine


def Render(filename, **kwargs):
  """A wrapper for template.render that handles some boilerplate.

  Args:
    filename: the basename (not the full path) of the template file.
    **kwargs: additional contents for the rendering dictionary.
  Returns:
    The rendered template text.
  """
  return template.render(os.path.join(_path, '.', filename), kwargs)


class _Handler(webapp.RequestHandler):
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


def _MachinesHelper(machines):
  found = _memcache.get_multi((str(m.keymeta) for m in machines),
                              key_prefix='meta-')
  for m in machines:
    key = str(m.keymeta)
    if key in found:
      yield m, found[key]
    else:
      yield m, m.most_recent_meta()


class ListMachines(_Handler):
  """Returns a list of available machines."""

  def get(self):
    """HTTP GET handler."""
    self.ValidateUser()
    machines = Machine.query().order(-Machine.modified_time).fetch(1000)
    self.Render('machines.djt', machines=_MachinesHelper(machines))


def _FilesHelper(files):
  running_size = 0
  end_time = None
  splitter = False
  for f in files:
    if running_size is not None and f.size is not None:
      running_size += f.size
    else:  # file created before we had a 'size' attribute
      running_size = None
    if running_size:
      yield f, '%.2fk' % (running_size/1024.), end_time, splitter
    else:
      yield f, '', end_time, splitter
    splitter = False
    for key, _ in f.flags:
      if key == 'version':  # reboot detected
        end_time = f.create_time_code
        running_size = f.size
        splitter = True


class ListFiles(_Handler):
  """Returns a list of available files."""

  #gpylint: disable-msg=W0221
  def get(self, machineid):
    """HTTP GET handler."""
    self.ValidateUser()
    machine = ndb.Key(Machine, machineid).get()
    if not machine:
      return self.error(404)
    #gpylint: disable-msg=E1103
    files = machine.files().order(-File.create_time).fetch(2000)
    self.Render('files.djt',
                machineid=machineid,
                files=_FilesHelper(files),
                firstfile=files and files[0])


def _AsyncBlobReader(blobkey):
  """Yield a series of content chunks for the given blobid.

  The first yielded entry is just ''; before yielding it, we start a
  background blobstore request to get the next block.  This means you can
  construct muple AsyncBlobReaders, call .next() on each one, and then
  iterate through them, thus reducing latency.

  Args:
    blobkey: the blobid of a desired blob.
  Yields:
    A sequence of uncompressed data chunks (the contents of the blob)
  """
  ofs = 0
  future = blobstore.fetch_data_async(blobkey, ofs, ofs + BUFSIZE - 1)
  yield ''
  cacheable = True
  while future:
    data = future.get_result()
    ofs += len(data)
    if len(data) < BUFSIZE:
      future = None
    else:
      future = blobstore.fetch_data_async(blobkey, ofs, ofs + BUFSIZE - 1)
      cacheable = False
    yield data
  if cacheable:
    _memcache.set('zblob-%s' % blobkey, data)


def _TrivialReader(data):
  """Works like _AsyncBlobReader, if you already have the data."""
  yield ''
  yield data


def _Decompress(dataiter):
  """Yield a series of un-gzipped content chunks for the given data iterator.

  This is a bit complicated because we need to make sure "gzip bombs" don't
  suck up all our RAM.  A gzip bomb is a very small file that expands to
  a very large file (eg. a long series of zeroes).  Any single chunk of
  the file can be a bomb, so we have to decompress carefully.

  Args:
    dataiter: an iterator that retrieves the data, one chunk at a time.
  Yields:
    A series of uncompressed data chunks.
  """
  dataiter.next()  # get it started downloading in the background
  decomp = zlib.decompressobj()
  yield ''
  for data in dataiter:
    #logging.debug('received %d compressed bytes', len(data))
    yield decomp.decompress(data, BUFSIZE)
    while decomp.unconsumed_tail:
      yield decomp.decompress(decomp.unconsumed_tail, BUFSIZE)
  yield decomp.flush(BUFSIZE)
  while decomp.unconsumed_tail:
    yield decomp.flush(BUFSIZE)


def _DecompressBlob(blobkey):
  """Yield a series of un-gzipped content chunks for the given blobid."""
  return _Decompress(_AsyncBlobReader(blobkey))


def _DecompressBlobs(blobkeys):
  """Like _DecompressBlob(), but for a sequence of blobkeys.

  We start prefetching up to PARALLEL_BLOBS blobs at a time for better
  pipelining.

  Args:
    blobkeys: a list of blobid
  Yields:
    A sequence of uncompressed data chunks, from each of the blobs in order.
  """
  iters = []
  blobkeys = list(str(i) for i in blobkeys)

  while blobkeys:
    needed = PARALLEL_BLOBS - len(iters)
    logging.warn('fetching %d\n', needed)
    want = blobkeys[:needed]
    blobkeys[:needed] = []
    found = _memcache.get_multi(want, key_prefix='zblob-')
    for blobkey in want:
      zblob = found.get(blobkey)
      if zblob:
        logging.warn('found: %r\n', blobkey)
        dataiter = _Decompress(_TrivialReader(zblob))
      else:
        logging.warn('not found: %r\n', blobkey)
        dataiter = _DecompressBlob(blobkey)
      dataiter.next()  # get it started downloading
      iters.append(dataiter)
    while iters:
      dataiter = iters.pop(0)
      for data in dataiter:
        yield data


def _AllArgs(req, keys):
  query = []
  if keys:
    for key in keys:
      for val in req.get_all(key):
        query.append((key, val))
  return query


def _ParseTime(s):
  if not s:
    return None
  try:
    return datetime.datetime.strptime(s, '%Y%m%d-%H%M%S')
  except ValueError:
    return datetime.datetime.strptime(s, '%Y%m%d')


class Download(_Handler):
  """Retrieves a given file (and all its successors) from the blobstore."""

  #pylint: disable-msg=W0221
  def get(self, metakey):
    """HTTP GET handler."""
    self.ValidateUser()
    start_time = time.time()
    machine = ndb.Key(Machine, metakey).get()
    if not machine:
      return self.error(404)

    start = _ParseTime(self.request.get('start'))
    end = _ParseTime(self.request.get('end'))

    self.response.headers.add_header('Content-Type', 'text/plain')

    # AppEngine is annoying and won't just let us serve pre-encoded zlib
    # encoded files.  So let's decompress it and let appengine recompress
    # it if the client supports it.
    #gpylint: disable-msg=E1103
    q = machine.files().order(File.create_time)
    if start:
      q = q.filter(File.create_time >= start)
    if end:
      q = q.filter(File.create_time < end)

    nbytes = 0
    blobids = (f.blobid for f in q.fetch(1000))
    for data in _DecompressBlobs(blobids):
      self.Write(data.replace('\0', ''))
      nbytes += len(data)
      if nbytes > MAX_UNCOMPRESSED_BYTES:
        self.Write('\n(stopping after %d bytes)\n' % nbytes)
        break
    end_time = time.time()
    logging.info('Download: %d bytes in %.2f seconds',
                 nbytes, end_time - start_time)


def _ScanBlob(blobid):
  flags = []
  size = 0
  last_chunk = ''
  for chunk in _DecompressBlob(blobid):
    size += len(chunk)
    for panic in re.findall(r'(?:Kernel panic[^:]*|BUG):\s*(.*)', chunk):
      flags.append(('panic', panic))
    if 'Restarting system.' in chunk:
      flags.append(('reboot', 'soft'))
    for ver in re.findall(r'SOFTWARE_VERSION=(\S+)', chunk):
      flags.append(('version', ver))
    for uptime in re.findall(r'\[([\s\d]+\.\d+)\].*\n[^\[]*\[[\s0.]+\]',
                             chunk):
      uptimev = float(uptime.strip())
      if uptimev > 0:
        flags.append(('uptime_end', str(int(uptimev))))
    if chunk:
      last_chunk = chunk
  # the last chunk should have the most recent uptime in it
  uptimes = re.findall(r'\[([\s\d]+\.\d+)\]', last_chunk)
  if uptimes:
    uptimev = float(uptimes[-1].strip())
    flags.append(('uptime', str(int(uptimev))))
  return size, flags


class Upload(_Handler, blobstore_handlers.BlobstoreUploadHandler):
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
    sync()

  #pylint: disable-msg=W0221
  def post(self, filename):
    """HTTP POST handler."""
    uploads = list(self.get_uploads('file'))
    if len(uploads) != 1:
      logging.error('Exactly one attachment expected; got %d.',
                    len(uploads))
      self.error(500)
    meta = _AllArgs(self.request, self.request.get_all('_'))
    blobid = uploads[0].key()
    size, flags = _ScanBlob(blobid)
    f = File.New(name=filename, blobid=blobid, size=size, meta=meta,
                 flags=flags)
    f.put_async()
    machine = f.machine()
    machine.put_async()
    _memcache.delete('meta-%s' % machine.keymeta)

    self.Redirect('/')
    sync()


class Regen(_Handler):
  """Make sure all objects are indexed correctly for latest schema."""

  def get(self):
    """HTTP GET handler."""
    self.response.headers.add_header('Content-Type', 'text/plain')

    def _handle(machinekeys):
      count = 0
      keys = machinekeys.keys()
      machines = ndb.get_multi(ndb.Key(Machine, key) for key in keys)
      for key, machine in zip(keys, machines):
        if not machine:
          Machine(id=key).put_async()
          count += 1
      machinekeys.clear()
      self.Write('created %d\n' % count)
      sync()

    machinekeys = {}
    for f in File.query().order(-File.create_time):
      f.size, f.flags = _ScanBlob(f.blobid)
      f.put_async()
      machinekeys[f.machine_key()] = 1
      if len(machinekeys) > 500:
        _handle(machinekeys)
    _handle(machinekeys)
    self.Write('ok\n')


class StartRegen(_Handler):
  """Start a Regen operation using the TaskQueue, which has a long timeout."""

  def get(self):
    """HTTP GET handler."""
    taskqueue.add(url='/_regen', method='GET')


def _Query(meta):
  return (File.query()
          .filter(File.meta == meta)
          .order(-File.create_time)
          .iter(keys_only=True, batch_size=1000))


class Query(_Handler):
  """Return a list of all File keys containing the given metadata."""

  def get(self, meta):
    """HTTP GET handler."""
    self.response.headers.add_header('Content-Type', 'text/plain')
    for k in _Query(meta):
      self.Write('%s\n' % k)


class DeleteAll(_Handler):
  """Delete all File keys containing the given metadata."""

  def get(self, meta):
    """HTTP GET handler."""
    ndb.delete_multi(_Query(meta))
    self.Write('ok\n')


wsgi_app = webapp.WSGIApplication([
    ('/', ListMachines),
    ('/_regen', Regen),
    ('/_start_regen', StartRegen),
    ('/_query/(.+)', Query),
    ('/_deleteall/(.+)', DeleteAll),
    ('/upload/(.+)', Upload),
    ('/([^/]+)/', ListFiles),
    ('/([^/]+)/log', Download),
], debug=True)
