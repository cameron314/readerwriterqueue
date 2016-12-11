from conans import ConanFile
import os

class ReaderWriterQueue(ConanFile):
    name = "readerwriterqueue"
    url = 'https://github.com/Manu343726/readerwriterqueue'
    license = "MIT"
    version = "1.0.0"
    exports = "*.h"
    build_policy = "missing"
    generators = 'cmake'

    def package(self):
        include_dir = os.path.join('include', 'readerwriterqueue')
        self.copy('readerwriterqueue.h', dst=include_dir)
        self.copy('atomicops.h', dst=include_dir)
