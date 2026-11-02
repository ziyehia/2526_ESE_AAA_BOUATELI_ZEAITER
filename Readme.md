# Init

## Commande du rapport cyclique par UART

La commande `SET_CCR XXXX`, validée par Entrée, permet de modifier les
comparaisons PWM des bras U et V. L'argument doit contenir uniquement des
chiffres décimaux (`0` à `9`). La plage autorisée est **0 à 4250 inclus**,
la borne supérieure correspondant au registre ARR de TIM1 dans la
configuration actuelle.

Après validation, la valeur X est appliquée aux deux canaux :

- `CCR1 = X` pour le bras U ;
- `CCR2 = ARR − X` pour le bras V.

Les sorties complémentaires CH1N et CH2N suivent les canaux principaux avec
le temps mort configuré dans TIM1. La commande agit sur le rapport cyclique ;
elle ne constitue pas à elle seule un asservissement de vitesse.

Par exemple, `SET_CCR 2550` applique CCR1 = 2550 et CCR2 = 1700. Le shell
confirme les valeurs par le message :

```text
CCR1=2550 CCR2=1700 MAX=4250
```

### Caractères acceptés et correction de la saisie

Le shell accepte les lettres non accentuées (`a` à `z`, `A` à `Z`), les
chiffres, les espaces, `_` et `=`. Ces caractères sont affichés pendant la
saisie. Backspace permet d'effacer un caractère tant que la ligne reste
valide. Les fins de ligne CR, LF et CRLF sont reconnues.

Les caractères supplémentaires, notamment `-`, `+`, `.` et les caractères
accentués comme `ç`, sont refusés. Un caractère refusé n'est pas affiché et
invalide toute la ligne. Le shell ignore alors les caractères suivants,
y compris Backspace, jusqu'à Entrée. Ce comportement peut donner
l'impression que la saisie est bloquée ; Entrée affiche l'erreur et rend
le prompt disponible pour une nouvelle commande.

Le tampon autorise **63 caractères utiles** : la dernière place est réservée
à la fin de chaîne. Une ligne trop longue est également rejetée entièrement.

### Validation et messages d'erreur

Une commande rejetée **ne modifie ni CCR1 ni CCR2**. Les valeurs supérieures
au maximum ne sont pas ramenées à la borne : elles sont refusées.

| Saisie suivie d'Entrée | Réponse du shell |
| --- | --- |
| `SET_CCR 4251` ou `SET_CCR 9999` | `Error: value out of range. CCR range: 0-4250.` |
| `SET_CCR 12a3` | `Error: enter digits only. CCR range: 0-4250.` |
| `SET_CCR` sans argument | `Error: enter one digits-only value. CCR range: 0-4250.` |
| `SET_CCR 100 200` | `Error: enter one digits-only value. CCR range: 0-4250.` |
| `SET_CCR -100` ou présence d'un caractère refusé | `Invalid or too long command` |
| Ligne dépassant 63 caractères | `Invalid or too long command` |

Une suite de chiffres dépassant la capacité du type entier est refusée avec
le message de dépassement de plage, si la ligne respecte la limite du tampon.

Avec cette loi de commande, **X = 0 n'est pas une commande d'arrêt** : les
rapports cycliques des bras valent idéalement 0 % et 100 %. Pour X = 2125,
les deux bras ont le même rapport cyclique et la tension moyenne idéale
entre U et V est nulle ; cela ne garantit pas l'arrêt mécanique du moteur.
