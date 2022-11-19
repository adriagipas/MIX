# mixala

**mixala** és un *script* senzill en Python 3 que permet generar
binaris (targetes perforades) a partir de codi assemblador de la
màquina MIX.

Per a consultar les opcions cal executar:
```
python mixala.py --help
```

L'*script* suporta tres modes:

- **ASCII**: mostra una representació de com quedaria el codi binari
  en memòria.

- **PUNCHCARD**: Codifica directament el codi binari resultant en
  targetes perforades (cada línia de text representa una targeta). És
  molt limitat perquè no totes les instruccions es poden codificar
  directament utilitzant el joc de caràcters de les targetes. Servix
  principalment per compilar el carregador.

- **DECK**: Codifica el codi binari resultant en targetes perforades
  (cada línia de text representa una targeta) emprant una codificació
  especial que ha de ser descodificada per un carregador. Aquest mode
  inserix al principi el codi del carregador. L'única restricció és
  que el programa no pot fer referència a adreces menor o iguals a
  100. És el mode recomanat per a compilar programes.

L'ús típic per a compilar un programa seria:
```
python mixala.py -i examples/table_primes.mixal -o table_primes.deck
```
