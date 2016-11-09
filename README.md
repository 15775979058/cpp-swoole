# cpp-swoole


Install swoole
====
```shell
git clone https://github.com/swoole/swoole-src.git
phpize
./configure
cmake .
#cmake -DCMAKE_INSTALL_PREFIX=/opt/swoole .
sudo make install
```

Build libswoole_cpp.so
====
```shell
mkdir build
cd build
cmake ..
make
sudo make install
```

Build example server
====
```shell
cd examples
cmake .
make
```

Run
===
```shell
./bin/server
```
