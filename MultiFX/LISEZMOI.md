# Studio Multi FX (VST3) : Wah > Harmonizer > Delay > Spring Reverb

Un seul plugin d'effet pour Cubase (Windows), avec 4 modules en serie et une interface complete.
Fonctionne sur pistes mono ou stereo.

## Obtenir le .vst3 sans rien installer (recommande) : GitHub Actions
1. Cree un depot GitHub gratuit et envoie-y tout ce dossier (y compris le dossier cache `.github`).
2. Onglet "Actions" > "Build VST3 (Windows)" : la compilation se lance toute seule (~10 min).
3. Telecharge l'artefact "Studio-Multi-FX-VST3-Windows" : il contient `Studio Multi FX.vst3`.
4. Copie-le dans `C:\Program Files\Common Files\VST3\`
5. Cubase : Studio > Gestion des plug-ins VST > Mettre a jour. Le plugin est dans les effets ("Studio Multi FX").

## Ou compiler toi-meme (Visual Studio 2022 + CMake + Git)
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
Resultat : `build\MultiFX_artefacts\Release\VST3\Studio Multi FX.vst3`

## Les modules (chacun a son bouton ON)
- WAH : Pedal (position du filtre, automatable dans Cubase), Auto (balayage automatique), Rate, Resonance, Mix.
- HARMONIZER : 2 voix de -12 a +12 demi-tons (defaut +4 et +7), niveau de chaque voix, Dry (son original).
- DELAY : Time (ms) ou Tempo Sync (1/2 a 1/16, pointe, triolet, suit le tempo de Cubase), Feedback, Damp, Ping-Pong, Mix.
- SPRING REVERB : Decay, Damp, Boing (dispersion du ressort), Mix.
- OUTPUT : niveau general.

Par defaut seuls Delay et Spring sont actifs (a 25 %). Tous les reglages sont automatisables.
Desactiver Delay/Reverb coupe leur queue immediatement.

## Test du moteur audio (optionnel)
    cmake -B build -DBUILD_DSP_TEST=ON && cmake --build build --target DspTest
