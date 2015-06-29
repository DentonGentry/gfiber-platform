#!/usr/bin/python

"""Tests for wifi_files.py."""

import os
import wifi_files
from wvtest import wvtest

wifi_files.filepath = '/tmp/wifi_files_test'

content_list = []
wifi_files.create_files_from_wifi_info(content_list)
wvtest.WVPASSEQ(os.listdir(wifi_files.filepath), [])

info_string = 'Station 01:02:03:04:05:06 Hello World'
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n'])

info_string = ('Station 01:02:03:04:05:07 Hello World\n'
               'Station 01:02:03:04:05:08 Hey World')
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n',
                               'Station 01:02:03:04:05:07 Hello World\n',
                               'Station 01:02:03:04:05:08 Hey World\n'])

info_string = 'yolo'
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n',
                               'Station 01:02:03:04:05:07 Hello World\n',
                               'Station 01:02:03:04:05:08 Hey World\n'])

info_string = ''
wifi_files.parse_interface(content_list, info_string)
wvtest.WVPASSEQ(content_list, ['Station 01:02:03:04:05:06 Hello World\n',
                               'Station 01:02:03:04:05:07 Hello World\n',
                               'Station 01:02:03:04:05:08 Hey World\n'])

wifi_files.create_files_from_wifi_info(content_list)
wvtest.WVPASSEQ(sorted(os.listdir(wifi_files.filepath)), ['01:02:03:04:05:06',
                                                          '01:02:03:04:05:07',
                                                          '01:02:03:04:05:08'])

content = ''
with open(wifi_files.filepath + '/01:02:03:04:05:06') as outfile:
  content = outfile.read()
wvtest.WVPASSEQ(content, 'Station 01:02:03:04:05:06 Hello World\n')

with open(wifi_files.filepath + '/01:02:03:04:05:07') as outfile:
  content = outfile.read()
wvtest.WVPASSEQ(content, 'Station 01:02:03:04:05:07 Hello World\n')

with open(wifi_files.filepath + '/01:02:03:04:05:08') as outfile:
  content = outfile.read()
wvtest.WVPASSEQ(content, 'Station 01:02:03:04:05:08 Hey World\n')

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
