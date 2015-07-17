#!/usr/bin/python

"""Tests for wifi_files.py."""

import json
import os
import shutil
import tempfile
import wifi_files
from wvtest import wvtest

wifi_files.filepath = tempfile.mkdtemp(prefix='wifi_files_test')

content_list = []
wvtest.WVPASSEQ(os.listdir(wifi_files.filepath), [])

info_string = ('Station 01:02:03:04:05:06 Hello World\n'
               '\tinactive time: 1000 ms\n'
               '\tfoo: bar')
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n'
                               '\tinactive time: 1000 ms\n\tfoo: bar\n'])

info_string = ('Station 01:02:03:04:05:07 Hello World\n'
               '\tinactive time: 1001 ms\n'
               '\tfoo: baz\n'
               'Station 01:02:03:04:05:08 Hey World\n'
               '\tinactive time: 1002 ms\n'
               '\tfoobar: boo')
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n'
                               '\tinactive time: 1000 ms\n\tfoo: bar\n',
                               'Station 01:02:03:04:05:07 Hello World\n'
                               '\tinactive time: 1001 ms\n\tfoo: baz\n',
                               'Station 01:02:03:04:05:08 Hey World\n'
                               '\tinactive time: 1002 ms\n\tfoobar: boo\n'])

info_string = 'yolo'
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n'
                               '\tinactive time: 1000 ms\n\tfoo: bar\n',
                               'Station 01:02:03:04:05:07 Hello World\n'
                               '\tinactive time: 1001 ms\n\tfoo: baz\n',
                               'Station 01:02:03:04:05:08 Hey World\n'
                               '\tinactive time: 1002 ms\n\tfoobar: boo\n'])

info_string = ''
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n'
                               '\tinactive time: 1000 ms\n\tfoo: bar\n',
                               'Station 01:02:03:04:05:07 Hello World\n'
                               '\tinactive time: 1001 ms\n\tfoo: baz\n',
                               'Station 01:02:03:04:05:08 Hey World\n'
                               '\tinactive time: 1002 ms\n\tfoobar: boo\n'])

content_dict = wifi_files.create_content_dict(content_list)
wvtest.WVPASSEQ(content_dict, {'01:02:03:04:05:06': {'foo': 'bar'},
                               '01:02:03:04:05:07': {'foo': 'baz'},
                               '01:02:03:04:05:08': {'foobar': 'boo'}})

wifi_files.create_files_from_content_dict(content_dict)
wvtest.WVPASSEQ(sorted(os.listdir(wifi_files.filepath)), ['01:02:03:04:05:06',
                                                          '01:02:03:04:05:07',
                                                          '01:02:03:04:05:08'])

content = ''
with open(wifi_files.filepath + '/01:02:03:04:05:06') as outfile:
  content = json.load(outfile)
wvtest.WVPASSEQ(content, {'foo': 'bar'})

with open(wifi_files.filepath + '/01:02:03:04:05:07') as outfile:
  content = json.load(outfile)
wvtest.WVPASSEQ(content, {'foo': 'baz'})

with open(wifi_files.filepath + '/01:02:03:04:05:08') as outfile:
  content = json.load(outfile)
wvtest.WVPASSEQ(content, {'foobar': 'boo'})

content_list = []
info_string = """Station 00:00:00:00:01
\tSomething: Station 00:00:00:00:10
\tAnother thing: 42
Station 00:00:00:00:02
\tSomething: Station 00:00:00:00:20
\tAnother thing: 43
"""
wifi_files.parse_interface(content_list, info_string)
answer = ["""Station 00:00:00:00:01
\tSomething: Station 00:00:00:00:10
\tAnother thing: 42
""", """Station 00:00:00:00:02
\tSomething: Station 00:00:00:00:20
\tAnother thing: 43
"""]

wvtest.WVPASSEQ(content_list, answer)

shutil.rmtree(wifi_files.filepath)
