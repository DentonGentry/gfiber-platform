from setuptools import setup

LONGDESC="""I2C Module Base Utilities.

Provides a wrapper around the i2c-tools driver.
"""

setup(name='i2c',
      version='1.0',
      description='i2c-tools wrapper.',
      license='Apache',
      author='Ke Dong',
      author_email='kedong@google.com',
      py_modules=["i2c"],
      long_description=LONGDESC,
      keywords="i2c",
      )
