# 4.1.1 - 2023-03-09

## Bug fixes

- Do not unconditionally break before attributes.
  (68cd9f2)
  (Anton Lindqvist)

- Reduce number of open file descriptors.
  (77ef18f)
  (Anton Lindqvist)

# 4.1.0 - 2023-03-07

## News

- Never break struct field access.
  (e90c90a)
  (Anton Lindqvist)

- Minimize indentation for function arguments with AlignAfterOpenBracket.
  (3808d91)
  (Anton Lindqvist)

## Bug fixes

- Honor cpp x macros after function implementations.
  (17c87c7)
  (Anton Lindqvist)

- In simple mode, only add braces when tokens are moveable.
  (00444ca)
  (Anton Lindqvist)

- In diff mode, emit missing new line.
  (26ad403)
  (Anton Lindqvist)

- Improve handling of trailing comments after binary operators.
  (e2746f4)
  (Anton Lindqvist)

- Handle unlimited ColumnLimit.
  (51359b6)
  (Anton Lindqvist)

# 4.0.0 - 2022-02-15

## Changes

- Start honoring clang format configuration, currently limited to a subset of
  all the style options.
  See the manual for further reference.
  (8fb32ce)
  (Anton Lindqvist)

- Improve diff mode when called from non repository root.
  (64d39eb)
  (Anton Lindqvist)

## News

- In simple mode, declarations like the following can now be split into one
  declaration per distinct type.
  (c48eca2)
  (Anton Lindqvist)

   ```
   # before
   struct foo *a, b;
   # after
   struct foo b;
   struct foo *a;
   ```

- In simple mode, ensure the static keyword comes first.
  (76b549a)
  (Anton Lindqvist)

- Detect fairly common volatile token alias.
  (197ccd1)
  (Anton Lindqvist)

- Trim trailing whitespace from comments.
  (c3db386)
  (Anton Lindqvist)

- In simple mode, put braces around a single statement spanning multiple lines.
  (97e3554)
  (Anton Lindqvist)

- Output NUL terminators in formatted source code.
  (4727393)
  (Anton Lindqvist)

- Replace usage of uthash.h with simple lookup tables.
  (2f6a5f7, 58a7f68, 22f99ea, 882cbac)
  (Anton Lindqvist)

- Add support for GNU statement expressions.
  (6d42ac4)
  (Anton Lindqvist)

- Add support for basic inline assembler.
  (bd1e3af, 76778d3)
  (Anton Lindqvist)

- Sort includes in simple mode.
  (9612032)
  (Anton Lindqvist)

## Bug fixes

- Fix detection of pointers wrapped in parenthesis.
  (49c66b7)
  (Anton Lindqvist)

- Allow empty expression in foreach macros.
  (4c7c898)
  (Anton Lindqvist)

- Fix inline assembler handling of parenthesis bug.
  (112ecdd)
  (Anton Lindqvist)

- Correct alignment of brace initializers.
  (0dc1c19)
  (Anton Lindqvist)

- Trim trailing new lines when the file ends with usage of cpp.
  (7526024)
  (Anton Lindqvist)

- Break of long expressions.
  (5add695)
  (Anton Lindqvist)

- Break long return expressions.
  (0b51384)
  (Anton Lindqvist)

- Trim right parenthesis as part of a expression.
  (deb21a1)
  (Anton Lindqvist)

- Fix indentation for brace initializers in expressions.
  (aa0ef35)
  (Anton Lindqvist)

- Trim right parenthesis as part of inline assembler.
  (03f7c4d)
  (Anton Lindqvist)

- Break long ternary expressions.
  (7ac100a)
  (Anton Lindqvist)

- Trim trailing new line(s) after brace initializers.
  (b47466b)
  (Anton Lindqvist)

- Remove excessive space after declarations using cpp.
  (d8eebef)
  (Anton Lindqvist)

- Add missing space after brace initializers followed by attributes.
  (b0502c3)
  (Anton Lindqvist)

- Break long attributes.
  (7e760c9)
  (Anton Lindqvist)

- In simple mode, preserve statement braces with trailing comments.
  (8afad71)
  (Anton Lindqvist)

- In diff mode, fix edge case when the source code only consists of macros and
  comments.
  (9463a1c)
  (Anton Lindqvist)

# 3.0.1 - 2022-08-14

## Bug fixes

- Fix regression using cpp in declarations.
  (0d117ed)
  (Anton Lindqvist)

- Stricter detection of loop constructs hidden behind cpp.
  (1859d3d)
  (Anton Lindqvist)

- Detect more types as part of declarations using cpp.
  (553c533)
  (Anton Lindqvist)

- Stricter detection of cdefs.h macros.
  (00cfdb8)
  (Anton Lindqvist)

- Fix brace initializers indentation while being part of an expression.
  (e860afa)
  (Anton Lindqvist)

- Fix alignment of brace initializers.
  (d3cb1cf)
  (Anton Lindqvist)

- Break long expression with intertwined comments.
  (aa7bbcf)
  (Anton Lindqvist)

- The sizeof operator without parenthesis can only be followed by a single
  token.
  (16aa6f9)
  (Anton Lindqvist)

- Do not trim right parenthesis if preceded with a C99 comment.
  (4280427)
  (Anton Lindqvist)

- Break long declarations.
  (8bdae2d)
  (Anton Lindqvist)

# 3.0.0 - 2022-08-09

## Changes

- Replace spaces in comment indentation.
  (d541ebf)
  (Anton Lindqvist)

- Honor no hard line before function annotation.
  (3f298c9)
  (Anton Lindqvist)

- Conditionally remove empty line in beginning of function implementation.
  (3e95fe5)
  (Anton Lindqvist)

## News

- Add support for usage of cdefs.h macros.
  (2757635, 836c2b9)
  (Anton Lindqvist)

- Add support for OpenSSL type macros such as STACK_OF.
  (b56e61e)
  (Anton Lindqvist)

- Improve recovery from broken source code.
  (2e12e6a, a097a13)
  (Anton Lindqvist)

- Add support for inline assembler.
  (4ef029d, 0ca3a92)
  (Anton Lindqvist)

## Bug fixes

- Fix alignment of variable declarations spanning multiple lines.
  (b3984fc)
  (Anton Lindqvist)

- Break variable declarations spanning multiple lines even earlier.
  (258ce60)
  (Anton Lindqvist)

- Fix break of long expressions.
  (c27db83, 40dedf7, 728f641, 67440df, a31dbf5)
  (Anton Lindqvist)

- Move comment(s) when moving braces.
  (b031d14)
  (Anton Lindqvist)

- Fix multiple diff mode bugs.
  (2ad115c, 3edecea, b9b470d, 10589bc, 5761b50)
  (Anton Lindqvist)

- Never break before return expressions.
  (c1e9cb1)
  (Anton Lindqvist)

- Fix comment followed by comment bug.
  (533639e, 831f79f)
  (Anton Lindqvist)

- Fix detection of mixed field and ordinary brace initializers.
  (6e60679)
  (Anton Lindqvist)

# 2.1.0 - 2022-06-01

## News

- Introduce simple declarations, separate uninitialized stack local variable
  declarations are now merged into a single declaration.
  (74f069b)
  (Anton Lindqvist)

   ```
   # before
   int a = 1, x, y;
   int z;
   # after
   int a = 1;
   int x, y, z;
   ```

- Align variable declarations spanning multiple lines.
  (d593084, bc966c0)
  (Anton Lindqvist)

## Bug fixes

- Fix struct fields alignment bug.
  (016e39b)
  (Anton Lindqvist)

- Align enum just like struct.
  (a030abe)
  (Anton Lindqvist)

- Do not confuse function call expression as a cpp declaration.
  (ddb8c6d)
  (Anton Lindqvist)

- Fix simple statement indentation bug.
  (0659e5d)
  (Anton Lindqvist)

- Fix break of long expressions.
  (28fc084)
  (Anton Lindqvist)

- Fix label indentation bug.
  (c57b45b)
  (Anton Lindqvist)

- Fix function pointer call parenthesis alignment bug.
  (251f634)
  (Anton Lindqvist)

- Fix switch case statement indent bug.
  (fe41357)
  (Anton Lindqvist)

# 2.0.0 - 2022-04-13

## Changes

- Align cpp line continuations, i.e. backslashes.
  (c1e20b7)
  (Anton Lindqvist)

## News

- Trim empty lines before else statements.
  (7033277)
  (Anton Lindqvist)

- Add simple mode (`-s`) intended to simplify the source code:
  * Removal of redundant parenthesis around return expressions:
    ```
    # before
    return (x);
    # after
    return x;
    ```
  * Removal of redundant curly braces around oneline statements:
    ```
    # before
    if (argc > 0) {
        usage();
    }
    # after
    if (argc > 0)
        usage();
    ```
  * Do not mix presence and absence of curly braces around if/else statements if
    at least one statement cannot fit on a single line:
    ```
    # before
    if (argc > 0) {
        int n = argc - 1;
        return n;
    } else
        return 0;
    # after
    if (argc > 0) {
        int n = argc - 1;
        return n;
    } else {
        return 0;
    }
    ```
  (eca73ec, 27f4087, 524dc73, 9ba092a)
  (Anton Lindqvist)

- Honor existing new lines among functions arguments.
  (4e4a38a)
  (Anton Lindqvist)

- Honor space before goto labels.
  (2c107cd)
  (Anton Lindqvist)

## Bug fixes

- Remove extra space before attribute succeeding struct declarations.
  (81ed102)
  (Anton Lindqvist)

- Do not confuse queue(3) macros as declarations.
  (33ed4b4)
  (Anton Lindqvist)

# v1.2.0 - 2022-01-12

## News

- Discard excessive whitespace from comments and cpp macros.
  (15502ff, 15680c8, 96b9f63, 83e8501)
  (Anton Lindqvist)

## Bug fixes

- Do not confuse loop constructs hidden behind cpp macros.
  (6fe7e69)
  (Anton Lindqvist)

# v1.1.0 - 2021-11-22

## News

- Handle windows line endings.
  (492f346)
  (Anton Lindqvist)

- Never break before the closing parens in a call expression.
  (12c89ba)
  (Anton Lindqvist)

- Trim empty lines after switch case statements.
  (8007b49)
  (Anton Lindqvist)

- Trim trailing whitespace from comments.
  (8f9a933)
  (Anton Lindqvist)

## Bug fixes

- Optionally skip new line after function implementation.
  (19357a0)
  (Anton Lindqvist)

- Multiple fixes related to handling of cpp branches.
  (2bdc511, efb1941)
  (Anton Lindqvist)

- Fix indent of statements after switch case statement.
  (95f2656)
  (Anton Lindqvist)

- Multiple fixes related to brace initializers.
  (2b2a8a0, f0e722f)
  (Anton Lindqvist)

- correct handling of do/while statements in diff mode.
  (5032313)
  (Anton Lindqvist)

# v1.0.0 - 2021-08-28

## News

- Handle and align x macros in brace initializers.
  (61f851e)
  (Anton Lindqvist)

- Handle and align cpp macros in brace initializers.
  (9086e2e)
  (Anton Lindqvist)

- Allow enum declarations on a single line.
  (02f2f34)
  (Anton Lindqvist)

## Bug fixes

- Preserve new lines before comments and cpp declarations at end of a statement
  block.
  (3f25572)
  (Anton Lindqvist)

- Only grow the buffer if more than half of the space is utilized.
  Prevents from running out of memory due to aggressive and redundant
  allocations while reading large files/diffs.
  (018d346)
  (Anton Lindqvist)

- Do not let a cpp branch inside a struct declaration end the current scope of
  alignment.
  (4a05336)
  (Anton Lindqvist)

- Long expressions in combination with optional new lines would not break under
  certain circumstances.
  (530d436)
  (Anton Lindqvist)

# v0.5.4 - 2021-08-18

## Bug fixes

- Fix several diff mode crashes.
  (1f086d1, 1f04936)
  (Anton Lindqvist)

- Handle mixed designated and positional initializers.
  (c882f03)
  (Anton Lindqvist)

- Fix function pointer type detection regression.
  (72edf7e)
  (Anton Lindqvist)

- Fix alignment of nested brace initializers on the same line.
  (b1744b0)
  (Anton Lindqvist)

# v0.5.3 - 2021-08-08

## Bug fixes

- Fix function pointer type detection regression.
  (5fe68c7)
  (Anton Lindqvist)

- Improve detection of unknown types.
  (af73977)
  (Anton Lindqvist)

- Preserve spaces around misplaced binary operators.
  (926a879)
  (Anton Lindqvist)

- Preserve new lines before comments placed at the end of a statement
  block.
  (8787cec)
  (Anton Lindqvist)

- Do not emit a new line before a field expression.
  (d622edf)
  (Anton Lindqvist)

# v0.5.2 - 2021-07-31

## Bug fixes

- Recognize unknown types as part of a cast expression.
  (63fec6e)
  (Anton Lindqvist)

- Do not confuse certain function calls and function pointers.
  (86fead2)
  (Anton Lindqvist)

# v0.5.1 - 2021-07-30

## Bug fixes

- Fix diff mode bug caused by a diff chunk only adding cpp directives.
  (b3d23d9)
  (Anton Lindqvist)

- Fix incorrect indentation for block comments followed by goto labels.
  (4de9d61)
  (Anton Lindqvist)

- Recognize function pointer types without arguments.
  (adaa6ee)
  (Anton Lindqvist)

# v0.5.0 - 2021-07-23

## News

- Add support for reformatting only changed lines given a diff.
  Inspired by clang-format-diff and others alike.
  (7dc490f)
  (Anton Lindqvist)

- Remove empty lines in the beginning and end of blocks.
  (440b5ab, 6d61b6f)
  (Anton Lindqvist)

- Minimize alignment for declarations.
  (d981eec)
  (Anton Lindqvist)

- Honor new lines after assignment operators.
  (8ced949)
  (Anton Lindqvist)

## Bug fixes

- Assume that everything fits while having pending new line(s).
  (0172dd5)
  (Alexandre Ratchov, Anton Lindqvist)

- Do not allow new lines before the opening brace as part of an if statement.
  (8b7824b)
  (Anton Lindqvist)

- Break long binary expressions even if spaces around the operator are absent.
  (db0787a)
  (Anton Lindqvist)

- Fix nested parenthesis alignment.
  (741c751, 6912015)
  (Anton Lindqvist)

# v0.4.0 - 2021-07-06

## News

- Honor no existing alignment among variable definitions and function
  prototypes.
  (1e5488c)
  (Anton Lindqvist)

- Honor alignment of stack local variables.
  (d9498ff)
  (Anton Lindqvist)

- Be more conservative while formatting brace initializers without
  alignment.
  (ec8666f)
  (Anton Lindqvist)

- Do not require spaces around certain binary operators.
  (6690597)
  (Anton Lindqvist)

# v0.3.0 - 2021-07-01

## News

- Reduce expression indentation even further, use 4 spaces disregarding
  parenthesis depth.
  (cb90511)
  (Anton Lindqvist)

  ```
  # before
  if (!(vmm_softc->mode == VMM_MODE_EPT ||
  	vmm_softc->mode == VMM_MODE_RVI))
  	return 0;

  #after
  if (!(vmm_softc->mode == VMM_MODE_EPT ||
      vmm_softc->mode == VMM_MODE_RVI))
  	return 0;
  ```

# v0.2.0 - 2021-07-29

## News

- Stop honoring honor blank lines while aligning struct and union
  fields.
  Effectively treating each struct and union declaration as one unit of
  alignment.
  (208e78c)
  (Anton Lindqvist)

  ```
  # before
  struct s {
  	const char	*p;

  	int	n;
  };
  
  # after
  struct s {
  	const char	*p;

  	int		 n;
  };
  ```

- Simplify expression parenthesis indentation logic, just use a soft
  indentation for each pair of parenthesis.
  (4738ed8, dfcb913)
  (Anton Lindqvist)

  ```
  #before
  if (!(vmm_softc->mode == VMM_MODE_EPT ||
  	    vmm_softc->mode == VMM_MODE_RVI))
  	return EINVAL;

  #after
  if (!(vmm_softc->mode == VMM_MODE_EPT ||
  	vmm_softc->mode == VMM_MODE_RVI))
  	return EINVAL;
  ```

- Honor new lines form the original source code in expressions as long
  as the same expressions fits within 80 columns.
  (6fd232e, 406debd, ceaeef8)
  (Anton Lindqvist)

- Allow switch case statements to be placed on the same line.
  (d7be26f)
  (Anton Lindqvist)

## Bug fixes

- Do not emit a space before the right brace as part of an array declaration.
  (0dae997)
  (Anton Lindqvist)

- Do not confuse else if and nested if statements.
  (4058af1)
  (Anton Lindqvist)

# v0.1.0 - 2021-06-17

## News

- First release.
  (Anton Lindqvist)
