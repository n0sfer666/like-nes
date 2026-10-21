# Проба PixelLab на монахе

[← к пакету](README.md) · промпты ChatGPT — [prompts.md](prompts.md)

Три пробы на бесплатных 40 генерациях, чтобы решить, нужна ли подписка: персонаж, удар, шаг.
Уходит около 18 генераций, остаток — на повторы. Подписка нужна, если шаг в пробе 3 годится в игру:
тогда промежуточные кадры не рисуются руками.

Названия полей сняты с кода сайта 2026-10-21. Если поле называется иначе, ищите по смыслу.

## 0. Вход

1. Открыть [pixellab.ai](https://www.pixellab.ai/) → **Try now** → зарегистрироваться. Карта не нужна.
2. Открыть [pixellab.ai/create](https://www.pixellab.ai/create). Все три пробы делаются здесь.
   Цена в генерациях видна у кнопки запуска.

## 1. Персонаж — около 10 генераций

1. Инструмент **Create S-XL image (Pro)**.
2. **Reference images**: `style/style-ref.png`, если лист стиля из [prompts.md](prompts.md) §1 уже
   есть. Нет — оставить пустым.
3. Размер: **128 × 128**.
4. **No Background**: включить.
5. **Color palette (Beta)** → **Paste hex colours**, вставить. **Max colours**: *No limit*.

   ```text
   #140c10 #2a1c24 #3b2420 #6e4232 #a8684a #d39a72 #f0c9a0 #2a2a33 #555566 #8f93a3 #d0d4dc #5a2a14 #8f4419 #c86b1f #f0a03a #ffd27a #3a2418 #5c3a22 #8a5a2e #b9853f #e0b45c #f6d27a #fff1c1 #ffffff
   ```

6. **Description**:

   ```text
   Side view, full body, facing right, standing still. An old wiry martial-arts monk about 96
   pixels tall: long white beard reaching the chest, long white eyebrows, a necklace of large dark
   wooden prayer beads, very wide saffron sleeves, slightly hunched, bald head, saffron robe tied
   with a rope belt, cloth wraps on the forearms and shins, straw sandals. Hard 1-pixel dark
   outline, stepped cel shading, warm golden rim light from the left, cool blue shadows.
   Fantasy-era materials only, no modern items.
   ```

7. Запустить. Придут 4 варианта, лучший — **Download** → **Image (.png)** → `monk/pixellab-stand.png`.

Если не так:
- Pro не запускается без подписки — инструмент **Create M-XL image**: тот же **Description**,
  размер **128 × 128**, **View**: *side*, **Direction**: *east*, **Transparent background**.
- Фигура мелкая или обрезана — дописать в начало `The figure fills the image height`.
- Пропала борода или чётки — новая генерация с той же **Description**. Пропадает дважды —
  вынести деталь в первую фразу.

## 2. Холст 192 × 192 под анимацию

Анимация берёт размер кадра у первого кадра, а клетка листа у нас 192 × 192.

1. Pixelorama: **File → Open...** → `pixellab-stand.png`.
2. **Project → Resize Canvas**: 192 × 192, якорь — низ-центр.
3. Подошвы — на нижнем ряду пикселей. Если между ними пустые строки, сдвинуть фигуру вниз
   инструментом **Move**.
4. **File → Export...** → **Image** → `monk/pixellab-stand-192.png`.

## 3. Удар — 3 генерации

1. Инструмент **Animate with text**.
2. **First frame**: `pixellab-stand-192.png`.
3. **Color palette (Beta)**: те же hex, что в пробе 1.
4. **Frame count**: 4. **Transparent background**: включить.
5. **Action description**:

   ```text
   palm strike: weight shifts back and the front palm is pulled to the shoulder, then the front
   arm extends fully at shoulder height with an open palm, rear hand at the chin
   ```

6. Запустить. **Show spritesheet** → сохранить `monk/pixellab-strike1.png`.

Если не так:
- Удар кулаком вместо ладони — в начало описания `open hand, fingers together,`.
- Фигура уезжает по кадру — в конец `the feet stay in place`.

## 4. Шаг — 5 генераций

1. **Animate with text**, **First frame**: `pixellab-stand-192.png`, та же палитра.
2. **Frame count**: 8. **Transparent background**: включить.
3. **Action description**:

   ```text
   walk cycle in place, side view facing right: heel contact, passing with the knee raised,
   arms swinging opposite to the legs, wide sleeves follow the arms, the body does not travel
   ```

4. Запустить. **Download as GIF** — посмотреть цикл, **Show spritesheet** → `monk/pixellab-walk.png`.

Если не так:
- Ноги скользят по земле — в описание `each foot stays planted while it carries the weight`.
- Цикл не замыкается — **Frame count** 6 или 10: последний кадр должен вести в первый.

## Итог

Прислать три листа (`pixellab-strike1.png`, `pixellab-walk.png`, `pixellab-stand.png`) и одну
фразу: брать подписку или нет. Шаг годится — PixelLab займёт шаг in-betweens в
[workflow.md](workflow.md), не годится — пакет остаётся на ChatGPT.
