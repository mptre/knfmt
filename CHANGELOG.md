# 4.4.0 - YYYY-MM-DD

## News

- Group blocks of includes.
  (c22119b)
  (Anton Lindqvist)

- Rework memory allocations.
  Speed up of 50% while formatting $openbsd-src/sys/kern/*.[ch].
  (bbfc385)
  (Anton Lindqvist)

- In simple mode, sort forward declarations.
  (c1b96d2)
  (Anton Lindqvist)

- Honor clang-format option BasedOnStyle.
  (8548ad1)
  (Anton Lindqvist)

- Honor clang-format option IncludeGuards.
  Note, this is a knfmt exclusive extension.
  (16c9f11)
  (Anton Lindqvist)

- In simple mode, introduce pass to correct implicit unsigned int types.
  (d8d7659)
  (Anton Lindqvist)

## Bug fixes

- Improve handling of expression arguments in declarations making use of cpp
  macros.
  (b72e19a)
  (Anton Lindqvist)

- Honor optional semicolon after extern blocks.
  (bb044a6)
  (Anton Lindqvist)

- Fix expression ternary precedence bug.
  (09c3137)
  (Anton Lindqvist)

# 4.3.0 - 2023-10-15

## News

- In simple mode, remove redundant parenthesis around all type of expressions.
  (964742f)
  (Anton Lindqvist)

- In simple mode, insert missing parenthesis around sizeof argument.
  (2796079)
  (Anton Lindqvist)

- Sense alignment in cpp macro definitions.
  (e46b3f5)
  (Anton Lindqvist)

- Detect more sys/cdefs.h like macros.
  (58911b5)
  (Anton Lindqvist)

- In simple mode, sort more blocks of includes.
  (911f0c7)
  (Anton Lindqvist)

- Indent long lists of inline assembly operands.
  (92d38a4)
  (Anton Lindqvist)

- Improve mimicking of clang-format alignment for statement expressions.
  (d2534fe)
  (Anton Lindqvist)

- Improve detection of loop statements hidden behind cpp macros.
  (2ea9d12)
  (Anton Lindqvist)

- Add line context to error messages.
  (3d072e1)
  (Anton Lindqvist)

- Improve handling of cpp macros in inline asm operands.
  (9aa415d)
  (Anton Lindqvist)

## Bug fixes

- In simple mode, do not remove braces around semicolon statement as such code
  can cause GCC to emit a warning.
  (8c465ec, e6ff77c)
  (Anton Lindqvist)

- Fix regression introduced while adding bool token aliases.
  (4da6ae7)
  (Anton Lindqvist)

- Make detection of types behind cpp macros more stringent.
  (e4cc1b5)
  (Anton Lindqvist)

- Make detection of attributes hidden behind cpp more stringent.
  (70889e0)
  (Anton Lindqvist)

- Treat form feed as a space.
  (2d5b76d)
  (Anton Lindqvist)

- Detect keywords and integers expressed as strings in .clang-format
  configuration files.
  (2a35d91, d46137f)
  (Anton Lindqvist)

- Honor clang-format option AlignAfterOpenBracket in cpp x macros.
  (deaf10d)
  (Anton Lindqvist)

# 4.2.0 - 2023-07-21

## News

- Never break before right parenthesis in expressions.
  (27a06b8)
  (Anton Lindqvist)

- Recognize restrict keyword.
  (f17f367)
  (Anton Lindqvist)

- Remove excessive semicolons after statements and declarations.
  (4528499, 9865f68)
  (Anton Lindqvist)

- Sense alignment in brace initializers and declarations.
  Instead of being picky, detect already aligned columns using either spaces or
  tabs and preserve such alignment.
  (dbc6d96, 000bcc0, cf7aa5f, 6bbd5af, a61fb35, b3ae100, ee08260, 05021b9,
   09b9566)
  (Anton Lindqvist)

- In simple mode, insert braces around cpp loop macro statement(s) spanning
  multiple lines.
  (4eb9095)
  (Anton Lindqvist)

  ```
  # before
  TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry)
  	if (prefix->tk_flags & TOKEN_FLAG_CPP)
  		return 1;
  # after
  TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
  	if (prefix->tk_flags & TOKEN_FLAG_CPP)
  		return 1;
  }
  ```

- In simple mode, insert braces around while statement(s) spanning multiple
  lines.
  (a5892e0)
  (Anton Lindqvist)

- Honor spaces before right brace in braces initializers.
  (d74fae5)
  (Anton Lindqvist)

- Only allow one consecutive new line in expressions.
  (b6323de)
  (Anton Lindqvist)

- Remove excessive new line(s) in brace initializers.
  (8395465)
  (Anton Lindqvist)

- In simple mode, improve placement of moved declarations.
  (7ab8ead)
  (Anton Lindqvist)

- In simple mode, detect usage of named and unnamed arguments in function
  prototypes.
  If the two conventions are mixed within a declaration, assume it's not
  intentional and all argument names are removed for consistency.
  (456b2a0)
  (Anton Lindqvist)

- In simple mode, add missing trailing comma in brace/designated initializers.
  (1611ddc)
  (Anton Lindqvist)

- Recognize goto labels in inline assembler.
  (10515e0)
  (Anton Lindqvist)

- Recognize attributes after return type in function prototypes and
  implementations.
  (96f0a8e)
  (Anton Lindqvist)

- Trim right parenthesis in for loops.
  (9056049)
  (Anton Lindqvist)

- Detect function attributes hidden behind cpp macros.
  (43829dd)
  (Anton Lindqvist)

- Improve alignment of binary expressions.
  (01a4399)
  (Anton Lindqvist)

- Detect leading attributes in function arguments.
  (5536f4a)
  (Anton Lindqvist)

- Honor new line(s) after comma in declarations.
  (b978e5c)
  (Anton Lindqvist)

- Detect more variations of foreach loops hidden behind cpp macros.
  (ed94215)
  (Anton Lindqvist)

- Detect leading attributes in function implementations.
  (8effce9)
  (Anton Lindqvist)

## Bug fixes

- In simple mode, fix sort includes bug when includes are grouped with other
  defines.
  (62504b0)
  (Anton Lindqvist)

- Fix indentation in brace initializers spanning multiple lines.
  (d7cb071)
  (Anton Lindqvist)

- In diff mode, fix bug related to cpp branches.
  (510beae)
  (Anton Lindqvist)

- Multiple bug fixes and improvements to clang-format option
  AlignAfterOpenBracket.
  (85de703, 379b7c2)
  (Anton Lindqvist)

- Multiple bug fixes and improvements to clang-format option
  BreakBeforeBinaryOperators.
  (c800ac3)
  (Anton Lindqvist)

- Make cast expression detection more strict, avoiding false positives.
  (34043aa, 7e59383)
  (Anton Lindqvist)

- Do not confuse binary operators and casts.
  (57e7a57)
  (Anton Lindqvist)

- Fix off by one during parenthesis alignment.
  (d8f88e3)
  (Anton Lindqvist)

- Fix indentation after C99 comments in expression.
  (11bf235)
  (Anton Lindqvist)

- Recognize asm, attribute, inline, restrict and volatile preceded or succeeded
  with any amount of underscores as the corresponding keyword without underscores.
  (1523fd1)
  (Anton Lindqvist)

- Do not trim redundant semicolon(s) from declarations in for loops.
  (3688b9b)
  (Anton Lindqvist)

- Add missing spaces after elements in brace initializers.
  (dedad51)
  (Anton Lindqvist)

- In simple mode, a semicolon statement is not considered empty when considering
  removing/adding braces.
  (9755ace)
  (Anton Lindqvist)

- Plug file descriptor leak during inplace edits.
  (164bb2a)
  (Anton Lindqvist)

- Better handling of sizeof with parenthesis.
  (928ecdc)
  (Anton Lindqvist)

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

# 4.0.0 - 2023-02-15

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

# 1.2.0 - 2022-01-12

## News

- Discard excessive whitespace from comments and cpp macros.
  (15502ff, 15680c8, 96b9f63, 83e8501)
  (Anton Lindqvist)

## Bug fixes

- Do not confuse loop constructs hidden behind cpp macros.
  (6fe7e69)
  (Anton Lindqvist)

# 1.1.0 - 2021-11-22

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

# 1.0.0 - 2021-08-28

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

# 0.5.4 - 2021-08-18

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

# 0.5.3 - 2021-08-08

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

# 0.5.2 - 2021-07-31

## Bug fixes

- Recognize unknown types as part of a cast expression.
  (63fec6e)
  (Anton Lindqvist)

- Do not confuse certain function calls and function pointers.
  (86fead2)
  (Anton Lindqvist)

# 0.5.1 - 2021-07-30

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

# 0.5.0 - 2021-07-23

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

# 0.4.0 - 2021-07-06

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

# 0.3.0 - 2021-07-01

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

# 0.2.0 - 2021-07-29

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

# 0.1.0 - 2021-06-17

## News

- First release.
  (Anton Lindqvist)
