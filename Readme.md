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

<img width="2880" height="2160" alt="image" src="https://github.com/user-attachments/assets/099a9956-e954-431d-bae2-e8ef631b20af" />


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


## 6.3 Premiers tests

Brancher le moteur en respectant les données constructeur du moteur.

Faites vérifier l'ensemble des signaux par votre professeur.

Faire un premier test dans les conditions suivantes (dans l'ordre) :

Rapport cyclique de 50%
Rapport cyclique de 70%
Quels problèmes observez vous ?

Pour palier à ce problème, générer une montée progressive du rapport cyclique jusqu'à arriver à la vitesse cible commandé par la commande définie précédemment.


On a branché le moteur en respectant les données constructeur du moteur

On a fait un premier test dans les conditions suivantes:
Rapport cyclique de 50%
Rapport cyclique de 70%

N'ayant pas eu le temps nous n'avons pas vu de problèmes. Comme attendu , à 50% le moteur ne tourne pas. à alpha entre 0 et 50% on passe de -E à 0V et entre 50 et 100% on passe de 0V à E.


## 7.2. Mesure de courant

### Courants à mesurer

Le schéma KiCad montre que la MCC est branchée entre les bras U et V (sorties `U_motout` et `V_motout` vers J102). Le courant d'induit vérifie donc i_MCC = i_U = −i_V. Une seule mesure suffit, mais on lit `U_Imes` et `V_Imes` : la moyenne (i_U − i_V)/2 réduit le bruit, et l'écart i_U + i_V ≠ 0 révèle une erreur d'offset ou de gain. `W_Imes` est inutile, car le bras W ne commute pas. `Bus_Imes` mesure le courant du bus continu (≈ (2α − 1)·i_MCC), utile pour un bilan de puissance mais pas pour l'asservissement.

### Fonction de transfert des capteurs

Les capteurs sont à effet Hall et bidirectionnels. Leur sortie est affine en courant :

V_out = V_ref + S × I

V_ref est la tension de sortie à courant nul, S la sensibilité en V/A ; les deux sont donnés par la datasheet du capteur. Côté STM32, avec un ADC 12 bits référencé à 3,3 V :

I = (N × 3,3 / 4095 − V_ref) / S

L'offset V_ref est mesuré au démarrage, avec les drivers désactivés, en moyennant environ 1000 conversions. La tolérance d'offset du capteur suffirait sinon à fausser les faibles courants.

### Broches utilisées

| Signal | Broche STM32 | Connecteur NUCLEO | Canal ADC |
| --- | --- | --- | --- |
| `U_Imes` | PA1 | CN7 pin 30 | ADC1_IN2 (ADC12) |
| `V_Imes` | PB0 | CN7 pin 34 | ADC1_IN15 |
| `W_Imes` | PB1 | CN10 pin 24 | ADC1_IN12 |
| `Bus_Imes` | PC2 | CN7 pin 35 | ADC1_IN8 (ADC12) |

`U_Imes` et `V_Imes` sont tous deux accessibles par ADC1 : un seul ADC en mode scan suffit.

### Mesure en polling

ADC1, canal 2 (PA1), déclenchement logiciel, temps d'échantillonnage de 47,5 cycles.

```c
HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
HAL_ADC_Start(&hadc1);
HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
uint32_t n = HAL_ADC_GetValue(&hadc1);
float i_u = (n * 3.3f / 4095.0f - v_ref) / S;
```

Essais : vitesse nulle (`SET_CCR 2125`, courant moyen ≈ 0), vitesse non nulle à vide (faible courant de frottement, de signe fixé par le sens de rotation), puis en charge, avec une résistance sur la machine synchrone couplée (le courant augmente quand la résistance diminue). Chaque valeur est comparée à une pince ampèremétrique. En polling, l'instant d'échantillonnage est aléatoire par rapport à la PWM, si bien que l'ondulation de découpage se superpose à la mesure sous forme de bruit.

### Acquisition Timer → ADC → DMA synchronisée sur la PWM

En PWM centrée, les commutations sont symétriques autour des sommets du compteur (CNT = 0 et CNT = ARR). L'ondulation de courant est triangulaire et symétrique : à ces deux instants, le courant passe par sa valeur moyenne, et l'on est le plus loin possible des commutations. On déclenche donc l'ADC sur l'événement de mise à jour de TIM1 :

- **TIM1** : TRGO2 = *Update Event*, Repetition Counter = 1. Sans RCR, l'update a lieu en haut et en bas du comptage, soit 40 kHz ; avec RCR = 1, un update sur deux est gardé, soit une mesure par période PWM (20 kHz).
- **ADC1** : déclenchement externe *Timer 1 Trigger Out 2 event* (`ADC_EXTERNALTRIG_T1_TRGO2`), front montant, mode scan avec 2 conversions (rang 1 : IN2/`U_Imes`, rang 2 : IN15/`V_Imes`), conversion continue désactivée, *DMA Continuous Requests* activé.
- **DMA** : ADC1 → mémoire, mode circulaire, demi-mot (16 bits).

```c
uint16_t adc_buf[2];

HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, 2);
// la PWM de TIM1 est déjà démarrée : chaque update lance une séquence

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    HAL_GPIO_WritePin(DBG_GPIO_Port, DBG_Pin, GPIO_PIN_SET);
    i_u = (adc_buf[0] * 3.3f / 4095.0f - v_ref_u) / S;
    i_v = (adc_buf[1] * 3.3f / 4095.0f - v_ref_v) / S;
    HAL_GPIO_WritePin(DBG_GPIO_Port, DBG_Pin, GPIO_PIN_RESET);
}
```

**Vérification à l'oscilloscope** : voie 1 sur une sortie PWM (par exemple `U_PWM_H`), voie 2 sur le GPIO de debug. On doit observer une impulsion par période (20 kHz), toujours à la même position, peu après le milieu de l'impulsion haute. Le décalage correspond à la durée des deux conversions (≈ 2 × 60 cycles ADC ≈ 3 µs à 42,5 MHz) et à la latence de l'interruption. Une impulsion qui glisse par rapport à la PWM indique que le déclenchement n'est pas pris sur TIM1.

