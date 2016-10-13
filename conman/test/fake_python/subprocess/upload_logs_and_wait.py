#!/usr/bin/python

"""Fake upload-logs-and-wait implementation."""

UPLOADED = False


def call():
  global UPLOADED
  UPLOADED = True
  return 0, ''


def uploaded_logs():
  global UPLOADED
  result = UPLOADED
  UPLOADED = False
  return result
