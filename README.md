=====
cycvt
=====



This set of archetypes have been develop to support CVT collaborators works.

This contains the following archetypes:

    - special_enrich: the enrichment facility has been developed based on the
      [cycamore](https://github.com/cyclus/cycamore) enrichment archetype, but
      allowing the tracking of isotopes other than u235 and u238.




special_enricht
===============

The special enrich archetypes is based on the cycamore::enrichement archetype
([see](https://github.com/cyclus/cycamore)). The Cycamore enrichement only
allows u235 and u238 to go into the product and send all the other nuclides
present in the feed into the waste. The special_enrich archetype allows user to
keep them (or a fraction of them) in the product.
To do so, one can specify for any nuclide (other than u235 and u238) the
relative enrichement ratio (relative to u235 enrichment ratio) to allows the
special enrichment archetypes to determine the amount of this special nuclide to
add to the product.
Example of configuration for the special enrich:
```
<enrich_efficiencies>
    <item>
        <comp>U236</comp>
        <eff>0.2</eff>
    </item>
</enrich_efficiencies>
```
In this case, the ratio of the enrichment (feed over product) for the u236 will
be 0.2 time the ratio of the enrichment (feed over product) of the u235.
The enrichment are considered for each nuclide as mass of nuclide over the
total mass.

Note: the author are aware of the accuracy limit of this mobilisation of a
multi-isotope enrichment, and this implementation do not aims to have any
realistic representation.


Math behind SEnricht
--------------------

with a Feed F, a target Product (P) enrichement (Pe) and few special nuclide A,B with respective enrichment effectiveness Ea and Eb (effectiveness relative to U235 enrichement).

We also considere the "Enrich" function that enrich a pure U235/U238 feed into enricht uranium (LEU or HEU).

1. build the equivalent F by replacing nucs a and b by U238:

`Feq = F - Fa*A - Fb*B + Fa*U238 + Fb*U238` where Fa and Fb are the respective content of nucs A and B in the feed F

2. Enrich the equivalent feed Feq to get the equivalent product Peq and the Feed required quantity:

`Peq = Enrich(Feq)`

3. Re-add the nucs A and B in the product:

`P = Peq + Fa/|F| * Ea E_U235 * A + Fa/|F| * Eb E_U235 * B - Fa/|F| * Ea E_U235 * U238 + Fa/|F| * Eb E_U235 * U238` where `E_U235 = Peq_U235 / ||Peq|| / Feq_U235 / ||Feq||` (the ratio of U235 assay between Peq and Feq)

 4. Compute the Tails assuming the mass required of feed is the same as the equivalent feed:

 `M_F = M_Feq`
 `T = F - P`
