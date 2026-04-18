# cpubar

dead simple per-core cpu load bars. single c file, no deps. made this bc i kept sshing into boxes and wanting a quick "is the cpu melting rn" without installing htop.

works on macos + linux.

```
$ cpubar
core 0  ▇▇▇▇▇▇▇▇▇░░░░░░░░░░░  42%
core 1  ▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇▇░░░░  82%
core 2  ▇▇▇░░░░░░░░░░░░░░░░░  15%
core 3  ▇▇▇▇▇▇▇▇▇▇▇▇░░░░░░░░  58%
(ctrl+c to quit)
```

## install

```
make && sudo make install
```

that puts it in `/usr/local/bin/cpubar`. override w/ `PREFIX=...` if u want.

uninstall:

```
sudo make uninstall
```

## usage

```
cpubar                 # once per second, forever
cpubar -i 0.5          # refresh every half second
cpubar -n 5            # print 5 frames then exit
cpubar -w 40           # wider bars
cpubar --help
cpubar --version
```

## how it works

- on macos it calls `host_processor_info` from mach
- on linux it reads `/proc/stat`
- diffs consecutive samples to get a % per core. standard stuff

## dev

```
make            # build
make test       # smoke tests
make clean
```

tested on apple clang 17 and gcc on ubuntu. compiles clean w/ `-Wall -Wextra -Werror`.

## license

MIT, see LICENSE
