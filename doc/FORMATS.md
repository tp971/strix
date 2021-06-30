# Formats

## Input Formats

A synthesis specification can be given to strix by an LTL formula together
with a splitting of propositions into output and input propositions. A full
description of the supported LTL formulas is given in the
[Owl formats description](https://gitlab.lrz.de/i7/owl/blob/master/doc/FORMATS.md).
To invoke Strix like this, give the formula and propositions by
```
strix --formula <FORMULA> --ins=<INPUTS> --outs=<OUTPUTS>
```
where `<FORMULA>` is the LTL formula, `<INPUTS>` a comma-separated list of input propositions
and `<OUTPUTS>` a comma-separated list of output propositions. The propositions in both lists
should be a partition of all atomic propositions appearing in `<FORMULA>`.
Instead of `--formula` also the short option `-f` can be used.
The LTL formula can be given in a file `<FILE>` as follows:
```
strix <FILE> --ins=<INPUTS> --outs=<OUTPUTS>
```

For example, a controller for a simple arbiter specification can be synthesized as follows:
```
strix -f "G (!grant0 | !grant1) & G (req0 -> F grant0) & G (req1 -> F grant1)" --ins="req0,req1" --outs="grant0,grant1"
```

Strix has no native support for TLSF specifications, but these can be used
after conversion with the [SyfCo](https://github.com/reactive-systems/syfco) tool.

## Output Formats

Strix supports the following output formats:

- Mealy machine (KISS or DOT format)
- AIGER circuit (AAG, AIG or DOT format)
- BDD (DOT format)
- Parity game (PGSolver format)

For any specification, Strix first outputs the realizability header, which is either `REALIZABILE` or `UNREALIZABLE`.
Then, if the specification is realizable and the option `--realizability` is not given,
the output of the controller in one of the above formats follows.
By default, the controller is written to the standard output,
but can be redirected to a file by specifying the option `-o <OUTPUT>`, where `<OUTPUT>` is the output file name.

**Mealy machine**

A Mealy machine is a finite state-based machine describing for each pair of current state and valuation
of input propositions a successor state and a valuation of output propositions. Strix supports
output of incompletely-specified Mealy machines with "don't-care" edges in the
[KISS2 format](https://ddd.fit.cvut.cz/prj/Benchmarks/LGSynth91.pdf),
with additional headers specifying the input and output signals.

To simply output the controller in KISS format, use the `-k` or `--kiss` option.
The controller can also be output in the [DOT format](https://www.graphviz.org/doc/info/lang.html)
for further visualization with the DOT tool of [Graphviz](https://www.graphviz.org/)
by additionally specifying the `--dot` option.

**AIGER circuit**

Strix also supports output of controllers as And-Inverter-Graphs (AIG) in the [AIGER format](http://fmv.jku.at/aiger/).
This is the default output format if no other output option is specified.
The AIG is by written in textual form (`aag` header) by default, can also be written in the binary format (`aig` header) by specifying the `--binary` option.
The AIG can also be output in DOT format for visualization by specifying the `--dot` option.

**BDD**

Strix can output the controller as a BDD. This is only supported in the DOT format for visualization by specifying the `--bdd` option.
The BDD is obtained with the [CUDD library](https://github.com/rakhimov/cudd) which supports complement edges, for interpreting
the output see this [CUDD Manual](http://web.mit.edu/sage/export/tmp/y/usr/share/doc/polybori/cudd/node3.html#SECTION000318000000000000000).

**Parity game**

Strix can also output of the intermediate parity game in [PGSolver format](http://www.win.tue.nl/~timw/downloads/amc2014/pgsolver.pdf).
This is enabled by the `--parity-game` option.
Note that this parity game can be much larger than the game in memory, as edge parities are converted to node parities and some implicit edges
are made explicit. The parity game can be solved by any tool supporting the PGSolver format, e.g. [Oink](https://github.com/trolando/oink).
