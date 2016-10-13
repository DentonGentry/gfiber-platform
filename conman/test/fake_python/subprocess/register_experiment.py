#!/usr/bin/python

"""Fake register_experiment implementation."""


REGISTERED_EXPERIMENTS = set()


def call(experiment):
  REGISTERED_EXPERIMENTS.add(experiment)
  return 0, ''
