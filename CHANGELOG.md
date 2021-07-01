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
