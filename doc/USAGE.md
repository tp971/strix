# Usage

Strix accepts specifcations as LTL formulas, for more details see the [formats description](FORMATS.md).

To check realizability of an LTL formula `LTL_FORMULA` together with two
comma-separated lists of input and output propositions `INS` and `OUTS`,
Strix should be invoked as follows:
```
strix --realizability -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To output a controller as a Mealy machine, use:
```
strix -k -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To output a controller as an AIGER circuit, simply use:
```
strix -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```
To find a small AIGER circuit by additional minimization of the Mealy machine and usage of different state labels, use:
```
strix --auto -f "LTL_FORMULA" --ins="INS" --outs="OUTS"
```

For example, a simple arbiter with two clients can be synthesized as follows:
```
strix -f "G (req0 -> F grant0) && G (req1 -> F grant1) && G (!(grant0 && grant1))" --ins="req0,req1" --outs="grant0,grant1"
```

Strix has many more options, to list them use `strix --help`.

To use Strix with TLSF specifications, a [wrapper script](scripts/strix_tlsf.sh) is provided, which assumes
that the [SyfCo](https://github.com/reactive-systems/syfco) tool is installed and which may be called as follows:
```
scripts/strix_tlsf.sh TLSF_INPUT.tlsf [OPTIONS]
```
