from conans import ConanFile, CMake
import os

version = os.getenv('CONAN_READERWRITERQUEUE_VERSION', '1.0.0')
user    = os.getenv('CONAN_READERWRITERQUEUE_USER', 'Manu343726')
channel = os.getenv('CONAN_READERWRITERQUEUE_CHANNEL', 'testing')

class TestReaderWriterQueue(ConanFile):
    settings = 'os', 'compiler', 'build_type', 'arch'
    requires = (
        'readerwriterqueue/{}@{}/{}'.format(version, user, channel),
        'cmake-utils/0.0.0@Manu343726/testing'
    )
    generators = 'cmake'

    def build(self):
        cmake = CMake(self.settings)
        self.run('cmake {} {}'.format(self.conanfile_directory, cmake.command_line))
        self.run('cmake --build . {}'.format(cmake.build_config))

    def test(self):
        self.run(os.path.join('.', 'bin', 'example'))
