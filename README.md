# knfmt

The
[knfmt(1)][knfmt]
utility formats source code files according to 
[style(9)][style]
from OpenBSD in a best effort fashion.
By default, source code is read from standard input and the formatted source
code is written to standard output:

	$ knfmt <knfmt.c

It can perform an in place edit by writing back the formatted source code:

	$ knfmt -i knfmt.c

It can produce a diff between the original and formatted source code:

	$ knfmt -d knfmt.c

In addition, it can given a diff only format the changed lines.
This option is compatible with the options mentioned above:

	$ git diff | knfmt -Dd

The implementation is further described in [DESIGN][design].

[design]: DESIGN
[knfmt]: https://www.basename.se/knfmt/
[style]: https://man.openbsd.org/style.9

## Installation

### OpenBSD

```
# pkg_add knfmt
```

### From source

The installation prefix defaults to `/usr/local` and can be altered using the
`PREFIX` environment variable when invoking `configure`:

```
$ ./configure
$ make install
```

## License

Copyright (c) 2021-2022 Anton Lindqvist.
Distributed under the ISC license.
