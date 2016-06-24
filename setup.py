from distutils.core import setup

OPTIONS_LONGDESC="""Command-line options parser.
With the help of an options spec string, easily parse command-line options.

An options spec is made up of two parts, separated by a line with two dashes.
The first part is the synopsis of the command and the second one specifies
options, one per line.

Each non-empty line in the synopsis gives a set of options that can be used
together.

Option flags must be at the begining of the line and multiple flags are
separated by commas. Usually, options have a short, one character flag, and a
longer one, but the short one can be omitted.

Long option flags are used as the option's key for the OptDict produced when
parsing options.

When the flag definition is ended with an equal sign, the option takes one
string as an argument. Otherwise, the option does not take an argument and
corresponds to a boolean flag that is true when the option is given on the
command line.

The option's description is found at the right of its flags definition, after
one or more spaces. The description ends at the end of the line. If the
description contains text enclosed in square brackets, the enclosed text will
be used as the option's default value.

Options can be put in different groups. Options in the same group must be on
consecutive lines. Groups are formed by inserting a line that begins with a
space. The text on that line will be output after an empty line.
"""

options_url = 'https://github.com/apenwarr/bup/blob/master/lib/bup/options.py'
setup(name='options',
      version='1.0',
      description='A command-line options parser.',
      license='BSD',
      author='Avery Pennarun',
      author_email='apenwarr@google.com',
      url='git://github.com/apenwarr/bup.git',
      download_url=options_url,
      py_modules=['options'],
      long_description=OPTIONS_LONGDESC,
      keywords='options',
     )


setup(name='experiment',
      version='1.0',
      description='Python implementation of the GFiber experiment framework.',
      author='Richard Frankel',
      author_email='rofrankel@google.com',
      py_modules=['experiment'],
      keywords='experiment',
     )
