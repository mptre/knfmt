# knfmt

The
[knfmt(1)][knfmt]
utility formats source code files according to 
[style(9)][style]
from OpenBSD.

[knfmt]: https://www.basename.se/knfmt/
[style]: https://man.openbsd.org/style.9

## Installation

### From source

The installation prefix defaults to `/usr/local` and can be altered using the
`PREFIX` environment variable when invoking `configure`:

```
$ ./configure
$ make install
```

## License

Copyright (c) 2021 Anton Lindqvist.
Distributed under the ISC license.
