## Calcul du DTG

Les données nécessaires ont été relevées sur le **schéma KiCad** de la carte (références des transistors et du driver, résistance de grille `RG = 30 Ω`) et dans les **datasheets** correspondantes. TIM1 est cadencé à `f_TIM = 24 MHz / 6 × 85 / 2 = 170 MHz`. En mode *center-aligned*, une période PWM dure `2 × ARR` cycles, d'où `ARR = 170·10⁶ / (2 × 20·10³) = 4250`, soit une résolution de `log₂(4250) ≈ 12 bits` (≥ 10 bits demandés). Pour un rapport cyclique de 60 %, `CCR1 = 0,6 × 4250 = 2550` et `CCR2 = 4250 − 2550 = 1700`.

Le temps mort doit couvrir le blocage d'un transistor avant la conduction de l'autre, soit `td(off) + tf − td(on) = 39 + 35 − 11 = 63 ns` d'après la datasheet (mesures à `RG = 5,1 Ω`), plus l'écart de propagation maximal du driver HCPL-315J, `PDD max = 350 ns`. Comme la carte utilise `RG = 30 Ω`, environ 5,9 fois la valeur de test, on a mis les temps de commutation à l'échelle : `td(off) + tf ≈ 436 ns` et `td(on) ≈ 65 ns`, ce qui donne `371 + 350 ≈ 720 ns`. On retient **1 µs** pour garder une marge face aux dispersions, à la température et à l'approximation linéaire en RG.

Avec `t_DTS = 1 / 170 MHz ≈ 5,88 ns`, la première plage de DTG (`DTG[6:0] × t_DTS`) plafonne à 747 ns. On utilise donc la deuxième, `t = (64 + DTG[5:0]) × 2 × t_DTS`, de pas 11,76 ns : `1000 / 11,76 = 85`, donc `DTG[5:0] = 85 − 64 = 21`. Avec les bits de plage `DTG[7:6] = 10`, on obtient **`DTG = 0x80 + 21 = 0x95 = 149`**, et l'on vérifie que `85 × 11,76 ns = 1 µs`.

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
Avec `CCR1=2125`, qui correspond à un rapport cyclique de 50%n,nous obtenons :

<img width="2880" height="2160" alt="image" src="https://github.com/user-attachments/assets/8c138497-4c55-43cb-b26e-1953992048d3" />

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
