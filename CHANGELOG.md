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

## Bug fixes

- Do not emit a space before the right brace as part of an array declaration.
  (0dae997)
  (Anton Lindqvist)

- Do not confuse else if and nested if statements.
  (4058af1)
  (Anton Lindqvist)

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

# v0.1.0 - 2021-06-17

- First release.
  (Anton Lindqvist)
