# Промпты пилота

[← к пакету](README.md) · порядок шагов — [workflow.md](workflow.md)

Каждый блок копируется в ChatGPT целиком. Над разделом — что приложить. Каждый раздел — **новый
чат**. В разделах 4–6 первое сообщение задаёт правила, а дальше каждый блок — отдельное сообщение
в том же чате.

## 1. Лист стиля

Приложить: ничего.

```text
Pixel art style sheet for a side-scrolling martial-arts beat 'em up set in a fantasy mountain
land at dawn. One landscape image on a flat neutral grey background. No text, no labels, no
frames, no watermark, no UI.

Show three things:
1. A strip of the stage: a mountain road through a small village with curved grey tiled roofs,
   a bamboo grove behind it, layered misty mountains far away, a low warm sun on the left.
2. One old martial-arts monk standing in profile facing right, about one third of the image
   height: bald, long white beard, saffron robe with very wide sleeves, dark prayer beads.
3. A row of small props on the ground: a wooden crate, a clay pot, a stone, a bowl of rice.

Pixel style: true low-resolution pixel art, every pixel the same size, hard edges, no
anti-aliasing, visible square pixels, nothing rotated off the pixel grid. 1-pixel outlines:
dark on the outside, a darker tone of the fill on the lit side, broken inside the figure.
Stepped cel shading with 4-5 tones per material. No gradients, no blur, no glow, no
photorealism, no 3D render look. Dithering only in the sky and the mist, never on the monk.

Light: low warm dawn sun from the left and slightly behind. Golden rim light on lit edges, cool
dusty-blue shadows, soft mist between depth layers. Far layers are lighter and cooler, near
layers darker and warmer. The monk has more contrast than the background: the background never
uses the two outline colours or pure white.

The colours below apply to everything except the flat grey background. Use only them, each
list from dark to light. No pure black, no other colours:
outline: #140c10 #2a1c24
shadows and ground mist: #1a1c2c #2c3350 #3f4a6e #5b6b8c
sky, zenith to sun: #4a5d8f #7d86b3 #c7a6b8 #f2c9a0 #ffe7b8
far mountains and mist: #7f8fae #a9b4c8 #d6dbe3
earth, wood and stone in warm light: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
highlights and sparks: #f6d27a #fff1c1 #ffffff
pines and shaded grass: #1f2e2a #2f4a3a #4c6b45 #7a9356
bamboo and young leaves: #3d5a2a #6b8c3a #a9bf5a #d8e08a
skin: #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0
white hair, grey metal, roof tiles: #2a2a33 #555566 #8f93a3 #d0d4dc
the monk's saffron robe: #5a2a14 #8f4419 #c86b1f #f0a03a #ffd27a
```

## 2. Model sheet монаха

Приложить: `style/style-ref.png`.

```text
Pixel art character model sheet for a side-scrolling beat 'em up, in exactly the style of the
attached style reference. One landscape image. Flat solid magenta background #FF00FF around the
figures, with a crisp edge: no soft or pink halo. No text, no labels, no frames, no ground
shadow.

Character: an old wiry martial-arts monk. Visible in every view: a long white beard reaching the
chest, long white eyebrows, a necklace of large dark wooden prayer beads, very wide saffron
sleeves. Slightly hunched, bald head, saffron robe tied at the waist with a rope belt, cloth
wraps on the forearms and shins, straw sandals, calm sharp eyes. He fights with open palms and
low kicks.

Fantasy-era materials only, no modern items, no logos.

Show the same character four times in one row, all four figures exactly the same height, the
feet of all four on one shared horizontal ground line, wide gaps between the figures:
(1) neutral standing, full profile facing right; (2) fighting stance, full profile facing right;
(3) three-quarter front view; (4) back view.

Pixel style: a game sprite 96 pixels tall, drawn as true low-resolution pixel art scaled up:
every pixel the same size, hard edges, no anti-aliasing. 1-pixel outline, dark on the outside, a
darker tone of the fill on the lit side. Stepped cel shading, 4-5 tones per material. No
gradients, no dithering, no blur. Warm golden rim light from the left, cool blue shadows.

Use only these colours, each list from dark to light. No pure black, no other colours:
outline: #140c10 #2a1c24
skin: #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0
beard and eyebrows: #2a2a33 #555566 #8f93a3 #d0d4dc
robe and sleeves: #5a2a14 #8f4419 #c86b1f #f0a03a #ffd27a
rope belt, prayer beads, wraps, sandals: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
rim light and highlights: #f6d27a #fff1c1 #ffffff
```

## 3. Model sheet рядового

Приложить: `style/style-ref.png` и `monk/model-sheet.png`.

```text
Pixel art character model sheet for a side-scrolling beat 'em up, in exactly the style of the
attached style reference and at the same pixel scale as the attached monk model sheet. The
bandit is a little taller than the monk: 104 pixels against the monk's 96. One landscape image.
Flat solid magenta background #FF00FF around the figures, with a crisp edge: no soft or pink
halo. No text, no labels, no frames, no ground shadow.

Character: a mountain-road bandit, lean build, quilted padded vest over bare arms, cloth
headband over dark hair, sash belt, loose trousers with wrapped shins, bare fists, aggressive
forward lean.

Fantasy-era materials only, no modern items, no logos.

Show the same character four times in one row, all four figures exactly the same height, the
feet of all four on one shared horizontal ground line, wide gaps between the figures:
(1) neutral standing, full profile facing right; (2) fighting stance with fists up, full profile
facing right; (3) three-quarter front view; (4) back view.

Pixel style: a game sprite 104 pixels tall, drawn as true low-resolution pixel art scaled up:
every pixel the same size, hard edges, no anti-aliasing. 1-pixel outline, dark on the outside, a
darker tone of the fill on the lit side. Stepped cel shading, 4-5 tones per material. No
gradients, no dithering, no blur. Warm golden rim light from the left, cool blue shadows.

Use only these colours, each list from dark to light. No pure black, no other colours:
outline: #140c10 #2a1c24
skin: #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0
hair: #2a2a33 #555566 #8f93a3 #d0d4dc
shin wraps: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
rim light and highlights: #f6d27a #fff1c1 #ffffff
the vest and the trousers, only these four blues: #1e2340 #33406e #4f67a3 #7f9fd6
the sash and the headband, only these four reds: #3a1420 #6b2232 #a83a44 #e0705a
The four blues and the four reds appear nowhere else: not on the skin, hair, wraps or outline.
```

## 4. Ключевые позы монаха

Приложить к первому сообщению: `monk/model-sheet.png`. Каждый ответ — `monk/keys-<клип>.png`.

```text
You will draw animation key poses for a side-scrolling beat 'em up. The character is the one on
the attached model sheet: keep the same design, proportions, colours and pixel scale in every
image of this chat. Do not draw anything for this message, reply "ready". Each next message
gives one clip and its poses.

Rules for every image:
- Flat solid magenta background #FF00FF, crisp edge, no pink halo. No text, no labels, no frames.
- The poses of one message in one row, left to right in the given order, all facing right, wide
  gaps between them.
- The same scale in every pose: standing height 96 pixels of the sprite; a crouch or a lean is
  lower only by the pose.
- One shared horizontal ground line: the feet are on it in every grounded pose, airborne poses
  are above it. The body stays over the same spot; it does not travel across the image.
- Full body visible in every pose, nothing cropped.
- True low-resolution pixel art: every pixel the same size, hard edges, no anti-aliasing.
  1-pixel outline, stepped cel shading, warm rim light from the left, cool blue shadows.
- No motion blur, no speed lines, no impact effects, no ground shadow.
- Never lose: the long white beard, the prayer beads, the very wide sleeves that follow the arm.

Use only these colours, each list from dark to light. No pure black, no other colours:
outline: #140c10 #2a1c24
skin: #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0
beard and eyebrows: #2a2a33 #555566 #8f93a3 #d0d4dc
robe and sleeves: #5a2a14 #8f4419 #c86b1f #f0a03a #ffd27a
rope belt, prayer beads, wraps, sandals: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
rim light and highlights: #f6d27a #fff1c1 #ffffff
```

```text
Clip idle, 2 poses:
1) relaxed hunched stance, hands loosely in front of the belly;
2) the same stance, chest slightly raised, beard lifted by breath.
```

```text
Clip walk, 2 poses:
1) contact: front heel down, rear toes down, arms swinging opposite;
2) passing: one leg straight under the body, the other knee raised, body at its highest.
```

```text
Clip jump, 3 poses:
1) crouch before the jump, knees deeply bent, arms back;
2) rising, legs extended down, arms up, sleeves trailing;
3) tucked in the air, knees to chest, sleeves fluttering.
```

```text
Clip strike1, 2 poses:
1) wind-up: weight back, front palm pulled to the shoulder;
2) front arm fully extended at shoulder height with an open palm, rear hand at the chin.
```

```text
Clip strike2, 2 poses:
1) wind-up: torso turned away, rear palm cocked at the hip;
2) rear arm fully extended with an open palm, torso turned toward the viewer.
```

```text
Clip strike3, 3 poses:
1) chamber: front knee raised to the chest, arms out for balance;
2) low side kick fully extended at knee height, torso leaning back;
3) recovery: kicking foot set down, palms in guard.
```

```text
Clip grab, 1 pose:
1) reaching forward with both hands at chest height, gripping a collar.
```

```text
Clip throw, 3 poses:
1) gripping with both hands, hips loaded, knees bent;
2) twisting the torso, pulling over the hip;
3) follow-through, arms extended forward and down, weight on the front foot.
```

```text
Clip hurt, 1 pose:
1) head snapped back, torso bent back, arms flung out, eyes shut.
```

```text
Clip fall, 2 poses:
1) knocked off the feet, body horizontal in the air, limbs trailing;
2) back hitting the ground, beard and sleeves still in the air.
```

```text
Clip down, 1 pose:
1) lying on the back on the ground line, beads spread on the chest.
```

```text
Clip getup, 2 poses:
1) rolling to one knee, one palm on the ground;
2) rising from the knee, hands coming up to guard.
```

## 5. Ключевые позы рядового

Приложить к первому сообщению: `bandit/model-sheet.png`. Каждый ответ — `bandit/keys-<клип>.png`.

```text
You will draw animation key poses for a side-scrolling beat 'em up. The character is the one on
the attached model sheet: keep the same design, proportions, colours and pixel scale in every
image of this chat. Do not draw anything for this message, reply "ready". Each next message
gives one clip and its poses.

Rules for every image:
- Flat solid magenta background #FF00FF, crisp edge, no pink halo. No text, no labels, no frames.
- The poses of one message in one row, left to right in the given order, all facing right, wide
  gaps between them.
- The same scale in every pose: standing height 104 pixels of the sprite; a crouch or a lean is
  lower only by the pose.
- One shared horizontal ground line: the feet are on it in every grounded pose, airborne poses
  are above it. The body stays over the same spot; it does not travel across the image.
- Full body visible in every pose, nothing cropped.
- True low-resolution pixel art: every pixel the same size, hard edges, no anti-aliasing.
  1-pixel outline, stepped cel shading, warm rim light from the left, cool blue shadows.
- No motion blur, no speed lines, no impact effects, no ground shadow.
- Never lose: the headband, the sash belt, the quilted vest, the wrapped shins.

Use only these colours, each list from dark to light. No pure black, no other colours:
outline: #140c10 #2a1c24
skin: #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0
hair: #2a2a33 #555566 #8f93a3 #d0d4dc
shin wraps: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
rim light and highlights: #f6d27a #fff1c1 #ffffff
the vest and the trousers, only these four blues: #1e2340 #33406e #4f67a3 #7f9fd6
the sash and the headband, only these four reds: #3a1420 #6b2232 #a83a44 #e0705a
The four blues and the four reds appear nowhere else: not on the skin, hair, wraps or outline.
```

```text
Clip idle, 2 poses:
1) fighting stance leaning forward, fists up at chest height, knees bent;
2) the same stance, shoulders slightly raised by breath.
```

```text
Clip walk, 2 poses:
1) contact: front heel down, rear toes down, fists up, leaning forward;
2) passing: one leg straight under the body, the other knee raised, body at its highest.
```

```text
Clip strike1, 2 poses:
1) wind-up: rear fist cocked by the ear, body leaning forward;
2) straight punch fully extended at face height, rear foot pushing off.
```

```text
Clip hurt, 1 pose:
1) head snapped back, torso bent back, arms flung out, eyes shut.
```

```text
Clip getup, 2 poses:
1) rolling to one knee, one fist on the ground;
2) rising from the knee, fists coming up to guard.
```

```text
Clip fall, 2 poses:
1) knocked off the feet, body horizontal in the air, limbs trailing;
2) back hitting the ground, the sash ends still in the air.
```

```text
Clip thrown, 2 poses:
1) flung through the air upside down, limbs flailing;
2) crashing shoulder-first into the ground.
```

```text
Clip down, 1 pose:
1) lying on the back on the ground line, arms spread.
```

```text
Clip held, 1 pose:
1) grabbed by the collar, feet barely touching the ground, hands clawing at the grip.
```

## 6. Фон деревни

Приложить к первому сообщению: `style/style-ref.png`. Каждый ответ —
`village/village-<номер слоя>-<что>.png`.

```text
You will draw the background layers of one game screen, a small mountain village at dawn, for a
side-scrolling game with parallax. Each layer is a separate image in exactly the style of the
attached style reference. Do not draw anything for this message, reply "ready". Each next
message names one layer and its colours.

Rules for every layer:
- Wide landscape image, seen straight from the side, no perspective toward the viewer.
- The horizon stays at the same height in every layer of this chat, the sun low on the left.
- Only the named layer. Everything in front of it or behind it is flat solid magenta #FF00FF
  with a crisp edge, no pink halo. The sky layer is the only one that fills the whole image.
- Layers 1, 2 and 3 tile horizontally: the right edge continues the left edge at the same height.
- Dawn light from the low sun on the left: warm ochre and gold on lit faces, dusty blue shadows,
  mist between depths. A farther layer is lighter, cooler and lower in contrast than a nearer one.
- True low-resolution pixel art: every pixel the same size, hard edges, no anti-aliasing,
  stepped shading, no gradients except stepped sky bands, no blur, no glow.
- No people, no animals, no text, no frames.
- Use only the colours listed in the message, each list from dark to light. Never pure black,
  never pure white.
```

```text
Layer 0, the sky. It fills the whole image: stepped horizontal colour bands from deep dusty blue
at the top to peach and pale gold near the horizon, a low sun on the left edge, a few long thin
clouds.
Colours:
sky, zenith to sun: #4a5d8f #7d86b3 #c7a6b8 #f2c9a0 #ffe7b8
sun and cloud rims: #f6d27a #fff1c1
```

```text
Layer 1, far mountain ridges fading into mist, pale cold blue-grey, snow on the highest peaks.
The ridges take the lower half of the image; above them is magenta.
Colours:
mist and snow: #7f8fae #a9b4c8 #d6dbe3
distant blue: #4a5d8f #7d86b3
shadowed faces: #3f4a6e #5b6b8c
```

```text
Layer 2, mid-distance mountain slopes covered with dark pine trees, mist pooling at their foot.
The slopes take the lower half of the image; above them is magenta.
Colours:
pines: #1f2e2a #2f4a3a #4c6b45 #7a9356
shadows: #1a1c2c #2c3350 #3f4a6e #5b6b8c
mist: #7f8fae #a9b4c8 #d6dbe3
```

```text
Layer 3, a small mountain village: two-storey wooden houses with curved grey tiled roofs, a low
stone wall, a wooden fence, laundry hanging on a line, all seen from the road side. The houses
take the lower 60 % of the image; above the roofs is magenta.
Colours:
wood, plaster and stone in warm light: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
roof tiles and grey stone: #2a2a33 #555566 #8f93a3 #d0d4dc
shadows: #1a1c2c #2c3350 #3f4a6e #5b6b8c
shrubs and moss: #1f2e2a #2f4a3a #4c6b45 #7a9356
laundry and mist: #7f8fae #a9b4c8 #d6dbe3
```

```text
Layer 5, foreground objects close to the camera, darker and warmer than everything behind them:
one wooden post with a paper lantern lit from inside near the left edge, one low bush near the
right edge, everything else magenta. The lantern is drawn with flat stepped tones, no glow. Both
objects together cover at most 15% of the image width. The post may rise high at the very edge;
anything away from the edges stays below one seventh of the image height, half the height of a
fighter, because the fight happens in the middle.
Colours:
wood: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
bush: #1f2e2a #2f4a3a #4c6b45 #7a9356
shadows: #1a1c2c #2c3350 #3f4a6e #5b6b8c
paper lantern: #5a2a14 #8f4419 #c86b1f #f0a03a #ffd27a
```

## 7. Материал дороги

Приложить: `style/style-ref.png`. Ответ — `village/road-swatch.png`.

```text
Pixel art texture swatch for a side-scrolling game, in exactly the style of the attached style
reference: packed dirt road with small stones, seen from the side and slightly from above. One
square image completely filled with the material, no objects, no edges, no horizon, no text.
Flat even lighting from the left, no strong shadows. The pattern repeats without visible seams.
True low-resolution pixel art: every pixel the same size, hard edges, no anti-aliasing, stepped
shading, no gradients, no blur.
Use only these colours, each list from dark to light:
dirt: #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c
shadows between stones: #1a1c2c #2c3350 #3f4a6e #5b6b8c
stones: #2a2a33 #555566 #8f93a3 #d0d4dc
```
