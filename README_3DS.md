# „Dune II“ (OpenDUNE) portas Nintendo 3DS konsolei

Modernus realaus laiko strategijos (RTS) žaidimo **„Dune II: The Building of a Dynasty“** portas, skirtas Nintendo 3DS konsolei (.3dsx ir .cia formatais), sukurtas OpenDUNE variklio pagrindu su dviejų ekranų palaikymu, patogiu fiziniu valdymu ir pilnu lietuviškos lokalizacijos palaikymu.

---

## 1. Ekranai

* **Žaidimas, viršus (400 × 240):** tik mūšio laukas, kursorius ir pažymėjimas, be sąsajos rėmų. Variklio 240 × 160 vaizdas (15 × 10 langelių) išdidinamas į visą ekraną.
* **Žaidimas, apačia (320 × 240):** auksinis dėmėtas fonas, 128 × 128 radaras, kreditai, energija, originalus pažymėto objekto paveikslėlis ir HP. Pažymėjus būrį rodomas vienetų skaičius ir bendra gyvybė. Komandų ir grupių mygtukų ekrane nėra. Radarą galima naudoti nuo pradžios; neatrastos vietos lieka paslėptos.
* **Pradžia:** originalus „Dune II“ pavadinimas viršuje, juodas pasirinkimų meniu su baltu tekstu ir raudona trikampe rodykle apačioje. „Load Game“ rodomas nuo pradžios, net jei dar nėra išsaugojimų. Šeimos pasirinkimas naudoja tą patį juodą meniu ir valdomas A, Enter arba lietimu.
* **Construction Yard ir įprastos gamyklos:** apačioje keturių stulpelių tinklelis, iki trijų matomų eilučių. Ilgesnis sąrašas slenka judant žemyn. Po tinkleliu vertikaliai yra „Build this“ ir „Resume game“. Viršuje — kreditai, kaina, originali WSA iliustracija; pastatams taip pat energija, gyvybė ir trumpas aprašymas.
* **Parinktys, išsaugojimų sąrašai ir patvirtinimai:** auksiniai pasirinkimų langai apačioje, antraštė arba originalus Mentato vaizdas viršuje. Išsaugojimo pavadinimo įvedimas ir Starport langas kol kas naudoja originalią sąsają.

Auksinis fonas ir rėmeliai braižomi kodu. Atskiros sąsajos spalvos nekeičia originalių pastatų, unitų, WSA iliustracijų ar radaro paletės.

## 2. Valdymas

| Valdiklis | Žaidime | Meniu |
|---|---|---|
| **Circle Pad** | Kursorius | Pasirinkimų navigacija |
| **D-Pad** | Kamera | Pasirinkimų navigacija; statybose — tinklelis |
| **A** | Spustelėti: pažymėti / judėti / pulti; Laikyti: būrio pažymėjimo rėmelis; pastatyti pastatą | Patvirtinti; tinklelyje — statyti pažymėtą objektą |
| **B** | Atšaukti veiksmą, rėmelį arba pažymėjimą | Grįžti / tęsti žaidimą |
| **Y** | Pastatui: pasirinkti ką statyti / atidaryti gamyklą; Kariams: saugoti poziciją (Guard) | — |
| **X / ZL / ZR** | Visada pažymėti visus karius (veikia net kai pažymėtas pastatas) | — |
| **R** | Pastatui: taisyti (Repair); Kariams: atsitraukti (Retreat) | — |
| **L + X/Y/A/B** | Išsaugoti grupę 1–4 | — |
| **R + X/Y/A/B** | Iškviesti grupę 1–4 | — |
| **Lietimas** | Radaras perkelia kamerą | Pasirinkti meniu ar statybų elementą |
| **START** | Parinkčių meniu | Uždaryti / grįžti |
| **SELECT** | Atidaryti savo Construction Yard, nepriklausomai nuo pažymėto objekto | — |

Kai pastatas paruoštas, pažymėkite Construction Yard ir dar kartą spauskite **A**, kad pradėtumėte jo pastatymą. **A** patvirtina vietą, **B** grąžina pastatą į Construction Yard.

---

## 3. Garso Sistema ir Lietuviška Lokalizacija

* **Garsas (3DS Hardware NDSP):**
  * Naudojamas Nintendo 3DS aparatinis garso procesorius (NDSP), leidžiantis atkurti balso pranešimus („Reporting“, „Acknowledged“, bazių užpuolimus) ir garso efektus tiesiogiai iš originalių ar lietuviškų `.VOC` failų.
* **Lietuviški rašmenys ir tekstai:**
  * Teksto atvaizdavimo variklis (`GUI_3DS_DrawText`) palaiko lietuviškas raides (Ą, Č, Ę, Ė, Į, Š, Ų, Ū, Ž).
  * Palaikomi lietuviški `ENGLISH.PAK` (arba `LITHUAN.PAK`), lietuviški balso failai ir perpiešti meniu.

---

## 4. Įdiegimas į Nintendo 3DS

1. **Failų paruošimas:**
   * Nukopijuokite `OpenDUNE3DS.3dsx` ir `OpenDUNE3DS.smdh` į SD kortelės katalogą `sdmc:/3ds/dune3ds/`.
   * Arba įdiekite `OpenDUNE3DS.cia` per **FBI** įrankį tiesiai į 3DS pagrindinį meniu (Home Menu).
2. **Žaidimo duomenų failai:**
   * Nukopijuokite originalius arba lietuviškus „Dune II“ duomenų failus (`.PAK`, `.VOC`, `.CPS` ir kt.) į `sdmc:/3ds/dune3ds/data/` arba `sdmc:/3ds/dune3ds/`.
   * Išsaugojimų aplankas `sdmc:/3ds/dune3ds/` sukuriamas automatiškai. Perkeldami iš ankstesnio `opendune` aplanko, perkelkite ir savo išsaugojimus.
   * Paieškos tvarka: SD `data/`, SD pagrindinis žaidimo aplankas, ROMFS `data/`, ROMFS šaknis. Aplankas pasirenkamas tik radus netuščią `DUNE.PAK` arba išpakuotą `ICON.ICN` (taip pat palaikomi vardai mažosiomis raidėmis).
3. **Paleidimas:**
   * Paleiskite žaidimą per „Homebrew Launcher“ arba tiesiai iš 3DS Home meniu.

---

## 5. Kompiliavimas iš išeities kodo

Reikalingi įrankiai: `devkitPro` (`devkitARM`, `libctru`, `3dstools`, `bannertool`, `makerom`).

```bash
export DEVKITPRO=/path/to/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH

make -f Makefile.3ds
```

Sėkmingai sugeneruojami:
* `OpenDUNE3DS.3dsx` (paleidžiamas per Homebrew Launcher)
* `OpenDUNE3DS.cia` (instaliuojamas į konsolės pagrindinį ekraną)
* `OpenDUNE3DS.smdh` (piktograma ir aprašymas)

## 6. Sąsajos patikra kompiuteryje

Kompozitoriaus integraciniai testai naudoja variklio I/O pakaitalus ir tikrina meniu, šeimos patvirtinimą, statybų tinklelio ribas, paletes ir paslėptų mygtukų nebuvimą:

```bash
clang -std=gnu11 -fsanitize=address,undefined -iquote include -iquote src scripts/test_3ds_ui.c -o /tmp/dune-ui-test
/tmp/dune-ui-test
```

Tai nėra konsolės ar emuliatoriaus patikra. Tikram paleidimui reikia originalių „Dune II“ duomenų failų; šiame projekte jų nėra.

### Intro ir garso regresijos

Intro/logotipų metu rodomas tik `SCREEN_0`, o title iliustracija iš `SCREEN_1` naudojama tik aktyviame title meniu. Taip animacijų darbiniai duomenys nepatenka į ekraną. Meniu mygtukai turi atskirą tamsią ir auksinę paletę, nepriklausančią nuo animacijos spalvų.

Vaizdo atnaujinimas papildomai nelaukia VBlank laikmačio callback'e. MIDI buferių eilė sumažinta iki maždaug 46 ms; vargonų harmonikos fazė apribota 16 bitų diapazonu, kad nebūtų skaitymo už sinuso lentelės ribų.

```bash
clang -std=gnu11 -fsanitize=address,undefined -I scripts/test_stubs -iquote include -iquote src scripts/test_3ds_midi.c -o /tmp/dune-midi-test
/tmp/dune-midi-test
```

### Pakartotinė kodo patikra (2026-10-02)

Visus host regresinių testų rinkinius paleiskite:

```bash
bash scripts/test_3ds.sh
```

Testai naudoja AddressSanitizer ir UndefinedBehaviorSanitizer. Jie apima sąsają, kalbos eilę, MIDI natų laikymą bei dalinio inicijavimo atminties atlaisvinimą, valdymo grupių gyvavimo ciklą, misijos būsenos išvalymą ir komandų ribas mažesniuose žemėlapiuose. Įrenginio NDSP/HID dalys pakeistos testų pakaitalais.

Per auditą sutvarkytas grupių sutrumpinimas, kai pirmas vienetas nebeprieinamas; grupės ir pasirinkimas išvalomi prieš pakartotinai naudojant vieneto pool vietą bei keičiant misiją. Statymo užbaigimas atnaujina radarą, o neužbaigta konstrukcija neblokuoja gamybos lango atidarymo. Komandos tikrinamos pagal konkretaus vieneto galimus veiksmus, jų formacija ribojama misijos žemėlapiu. Lietimo paspaudimas laikomas iki atleidimo, o paskutinės koordinatės išsaugomos; A meniu nebesiunčia dviejų patvirtinimų. Pataisyti maži tekstų buferiai ir nesaugus gamybos objekto sukūrimo rezultato naudojimas.

Pilnas ARM perkompiliavimas apima visus 88 šio 3DS build C failus. Papildoma GCC `-fanalyzer` analizė atlikta 15 portavimo ir pagrindinių variklio failų. Tai neįrodo, kad visas žaidimas neturi klaidų: tikras intro, garsas, SD failų prieiga ir pilna misija dar turi būti patikrinti konsolėje su originaliais duomenimis.

### Pataisymai po kodo peržiūros (2026-10-30)

Atkurtas visų host testų paleidimas ir atnaujintos pasenusios sąsajos prielaidos. Dabar yra penki rinkiniai: sąsaja, MIDI, kalba, vienetų valdymas ir SD/ROMFS paieška. Pastarasis tikrina ir tuščio išsaugojimų aplanko atvejį. „Area Guard“ ir „Ambush“ leidžiami vienetams su „Guard“ veiksmu; siunčiant grupę į pastatą formacijos poslinkiai netaikomi, o judant per vietovę išlieka. `L + Y` nebesiunčia papildomo „Stop“.

### Apatinis taktinis skydelis pagal Figma

Vieneto ir pastato išdėstymas perkeltas iš Figma mazgų `58:1713` ir `58:1918`. Esama DOS antraštė, radaras ir skiriamasis stulpelis išlaikyti; dešinėje naudojamas `#B9BE91` fonas iki ekrano apačios. Koviniams vienetams paliktas vienas bendras „ATTACK / MOVE (A)“ mygtukas ir atskiri „RETREAT“ bei „GUARD“. Jo lietimo zona vykdo kontekstinę komandą ties viršutinio ekrano žymekliu. Specialių vienetų komandos išlaikytos. Pastatai turi „REPAIR“ ir didelį gamybos bloką su originaliu paveikslėliu bei progreso tinkleliu. Gamybos metu užrašas rodo atšaukimą, o paruošus pastatą – statymą.

Taktiniame skydelyje naudojami originalūs `INTRO.FNT`, `NEW8P.FNT` ir prireikus `NEW6P.FNT` glifai. Originalūs paveikslėliai telpa savo vietose išlaikant proporcijas; kai telpa, naudojamas sveiko skaičiaus didinimas. Skydelio spalvos nepriklauso nuo žaidimo paletės, o paveikslėliai ir radaras ją išlaiko.

### Bendras sąsajos ir valdymo pataisymas

- Išsaugojimo aprašymas įvedamas per Nintendo 3DS `swkbd` programėlę. Senas DOS teksto langas nebekuriamas. Atšaukus tekstas neišsaugomas, o patvirtinus grąžinamas tikras failo išsaugojimo rezultatas.
- Taktinio skydelio užrašams visur naudojamas aiškus originalus `NEW8P.FNT`, kaip geltonoje pranešimų juostoje. Raidžių kontūro ir šešėlio indeksai nebedažomi pagrindine teksto spalva.
- D-pad slinkimas turi pirmenybę prieš žymeklį priešingame krašte; pakeitus kryptį kartojimo delsa pradedama iš naujo. Senas DOS kraštų slinkimas 3DS versijoje išjungtas, kad nekonkuruotų su HID valdymu. Dialogų pasirinkimai perduodami tiesiogiai į variklio įvykių apdorojimą.
- Kelių vienetų skydelis turi 3 stulpelius ir 4 eilutes. Iki 12 vienetų rodomi visi; kai daugiau, rodomos 11 kortelių ir paskutinėje vietoje `+N`. Kiekviena kortelė turi savo gyvybių juostą ir leidžia pirštu pasirinkti vieną vienetą.
- Aplink vieneto portretą nebeliko pridėto juodo išorinio fono. Tarpai aplink veiksmų mygtukus ir tarp jų užpildyti juodai.

`bash scripts/test_3ds.sh` apima 6 testų rinkinius su ASan/UBSan: UI (įskaitant tikrą HID apklausos kodą), MIDI, kalbą, vienetus, katalogus ir sisteminės klaviatūros sąsają. Emuliatoriuje su vietiniais originaliais duomenimis patikrintas aiškus šriftas, penkių vienetų kortelės ir sisteminio teksto lango atidarymas, įvedimas bei atšaukimas. Fiziniame 3DS šie pakeitimai dar netikrinti.

### Mentat, misijos pabaiga ir tikslus valdymas (2026-10-30)

- Mentat sąrašas valdomas ir Circle Pad, ir D-pad; A atidaro pasirinktą temą. Mygtukai naudoja tamsų „Exit“ stilių su auksiniu tekstu ir paryškintu pasirinkimo rėmeliu. Taktinės komandos išlaiko padidintą `NEW8P.FNT`, kiti mygtukai naudoja originalų `INTRO.FNT` (trumpos eilutės – `NEW8P.FNT`).
- Circle Pad turi 20 vienetų nejautros zoną, laipsnišką greičio kreivę ir trupmeninių pikselių kaupimą. Ties centru galima tiksliai nutaikyti; maksimalus greitis 3 pikseliai per apklausą, atleidus judėjimas iškart sustoja.
- X pažymi savo matomus vienetus, įskaitant dalinai išlindusius už ekrano krašto. Antras X per 24 GUI laikmačio tiksnius grąžina kamerą prie jau pažymėto būrio. Talpa atitinka vienetų pool dydį, todėl pasirinkimas nebesutrumpinamas ties 32.
- Modalūs pranešimai išsaugo abiejų ekranų vaizdą ir UI paletes; apačioje rodomas pranešimas bei „Continue (A)“. Misijos pabaigos pranešimas parodomas prieš pereinant į Mentat režimą. Rezultatų ir rekordų ekranai turi atskirą dviejų ekranų režimą, o apatiniai rekordų mygtukai valdomi A, kryptimis ir lietimu.
- Visi `GUI_EditBox` įvedimai naudoja native `swkbd`: rekordų vardas (5 simboliai), saugumo atsakymas (80) ir išsaugojimo aprašymas (50). Atšaukus naujo rekordo vardą naudojamas „Pilot“, kad nereikėtų pakartotinai atidarinėti klaviatūros.
- Harvesterio iškrovimo, išvažiavimo ir veiksmų atkūrimo regresija vykdo tikras variklio funkcijas bei veiksmų lenteles. Patikrintos trys pagrindinės šeimos, pilno krovinio iškrovimas, užblokuotas išvažiavimas ir pakartotinis bandymas. Sėkmingai išvažiavęs harvesteris gauna `HARVEST`; užstatytam išvažiavimui reikia laisvos vietos.

`bash scripts/test_3ds.sh` dabar paleidžia 7 ASan/UBSan rinkinius. Automatinės patikros nepakeičia pilno kampanijos perėjimo fiziniame 3DS.
