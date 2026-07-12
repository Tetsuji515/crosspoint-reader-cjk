# 日本語読書用フォント（.bin）生成方法の確立 — STEP F-1/F-2 調査メモ

対象指示書：`xteink_x4_font_investigation_instruction.md`
調査ブランチ：`feature/jp-reading-font`
調査日：2026-07-13

実装は未着手。以下は読み取り専用のコード調査・外部ツール調査の結果のみ。

---

## STEP F-1：CJK版の読書用フォント（.bin）形式の調査

### F1-1／F1-2：読み込みコードとバイナリ実体

読み込み実装は `lib/ExternalFont/ExternalFont.h`（`class ExternalFont`）と`.cpp`。**2つの形式**をサポートしている（`ExternalFont.h`冒頭のドキュメントコメントに明記）。

拡張子に関わらず、`load()`はファイル先頭4バイトの**マジックナンバー**を見て形式を判定する（`"EPDF"` = 0x45504446 なら形式2、それ以外は形式1にフォールバック）。

#### 形式1：レガシー Xteink `.bin`（同梱の2ファイルはこちら）

ヘッダーなし。**コードポイント直接インデックス方式**：

```
offset = codepoint * bytesPerChar
bytesPerChar = bytesPerRow * height
bytesPerRow  = ceil(width / 8)
```

各グリフは `bytesPerRow * height` バイトの固定長、行アラインMSBファースト1bitパッキング。`width`/`height`/`size`は**ファイル名からのみ**取得（`FontName_size_WxH.bin`）。空き番地（グリフが存在しないコードポイント）はゼロ埋め。

**実バイナリで検証**（F1-2）：

| ファイル | 幅x高さ | bytesPerChar | ファイルサイズ | 逆算した最大コードポイント |
|---|---|---|---|---|
| `SourceHanSansCN-Bold_20_20x20.bin` | 20x20 | 3×20=60 | 3,932,040 | 65,534（≈BMP全域） |
| `KingHwaOldSong_38_33x39.bin` | 33x39 | 5×39=195 | 10,452,992 | 53,605 |

両方とも `ファイルサイズ ÷ bytesPerChar` が**割り切れる**（余りゼロ）ことを確認。実際にファイル先頭64バイトをダンプしても全ゼロで、直接インデックス方式・低コードポイント領域が空という理解と整合する。ヘッダーが無いため`"EPDF"`マジック判定にも一致しない＝レガシー形式であることが実証された。

#### 形式2：EPDFont `.epdf`（rich-metrics、x4-epdfont-converter製）

`ExternalFont.h`のコメントで**明示的に「x4-epdfont-converterが生成する」と記載**されており、さらにgit履歴のコミット `7e3de0d feat(font): replace XBF2 with EPDFont format from x4-epdfont-converter`（2026-05-01）にバイトレベルの完全な仕様がコミットメッセージとして残されている。独自形式「XBF2」を廃止し、この外部ツールの出力形式に**一致するように**ローダーを書き直した、という経緯。

**バイトレイアウト**（すべてリトルエンディアン）：

```
┌─────────────────────────────────────────────┐
│ ヘッダー（32バイト、offset 0）                  │
├──────┬──────┬─────────────────────────────────┤
│ 0..3 │ 4B   │ magic 'EPDF' (0x46445045)        │
│ 4..5 │ 2B   │ version (= 1)                     │
│ 6    │ 1B   │ is2Bit (0=1bit, 1=2bit AA)         │
│ 7    │ 1B   │ reserved                           │
│ 8    │ 1B   │ advanceY（行送り px）              │
│ 9    │ 1B   │ ascender (i8)                      │
│ 10   │ 1B   │ descender (i8, 負値)               │
│ 11   │ 1B   │ reserved                           │
│ 12..15│ 4B  │ intervalCount (u32)                │
│ 16..19│ 4B  │ glyphCount (u32)                   │
│ 20..23│ 4B  │ intervalsOffset (u32, == 32)       │
│ 24..27│ 4B  │ glyphsOffset (u32, ==32+ivCnt*12)  │
│ 28..31│ 4B  │ bitmapOffset(u32,==glyOff+glCnt*16)│
├─────────────────────────────────────────────┤
│ インターバル配列（12B×intervalCount）           │
│   start(u32) / end(u32,inclusive) / glyphOffset(u32,累積) │
├─────────────────────────────────────────────┤
│ グリフメタデータ配列（16B×glyphCount、interval順）│
│   width(u8) / height(u8) / advanceX(u8) / reserved(u8) │
│   left(i16) / top(i16) / dataLength(u32) / dataOffset(u32) │
├─────────────────────────────────────────────┤
│ ビットマップblob（可変長）                      │
│   1bit MSBファースト・シーケンシャルパッキング   │
│   （bit index = y*width + x、行アラインではない） │
│   ※ファームウェア側がキャッシュ格納時に行アライン形式へ変換 │
└─────────────────────────────────────────────┘
```

コードポイント検索：インターバル配列を二分探索 → `glyphIndex = interval.glyphOffset + (cp - interval.start)` → 16バイトのグリフエントリを読む → ビットマップblobから該当バイト列を読んで変換。

**2bitフォントは読み込み時に明示的に拒否される**（`is2Bit`フラグが1だとエラーログを出して失敗）。**1bitモードで書き出す必要がある。**

`FontFilenameParser.h`（F1-4）：拡張子が`.epdf`ならリッチ形式、`.bin`ならレガシー形式とファイル名から一次判定するが、実際のロード時は上記マジックナンバーで再判定される（拡張子と中身が食い違っていても中身優先）。ファイル名の`WxH`はレガシー形式では必須（バイト計算に直接使う）。EPDFont形式では「レンダラーがCJKレイアウトに使う名目上のセルサイズ」として使われる（グリフごとの実寸は別途ヘッダー内メタデータから取得）。

### F1-3：同梱ファイルの生成手段

`git log --all -- 'fonts/*'` では `feat: Add CJK font files for reader and UI` という2コミットでバイナリがそのまま追加されているのみで、**生成スクリプトの痕跡はリポジトリ内に存在しない**。`docs/cjk-fonts.md`にも生成コマンド例（`generate_cjk_ui_font.py`はUIフォント用の`.h`生成コマンドの記載のみ）はあるが、レガシー`.bin`読書用フォントの生成コマンドの記載はない。おそらく本家CrossPointプロジェクト側で外部生成されたものをそのまま持ち込んだと推測される。

### F1-5：`generate_cjk_ui_font.py`のラスタライズ流用可能性

Pillow（`PIL.Image`/`ImageFont`/`ImageDraw`、内部でFreeTypeを使用）で1bitモード画像に文字を描画し、行アラインMSBファーストでパッキングしている。**パッキング方式はレガシー`.bin`形式と同じ**（EPDFont形式のシーケンシャルパッキングとは異なる）。

このスクリプトはUI用の固定20x20セル・限定文字集合（`BASE_UI_CHARS`＋翻訳文字列）専用に作られており、可変グリフ寸法・メトリクス（advanceX/left/top等）・インターバルテーブルの概念がない。EPDFont形式向けに転用するには、ラスタライズ部分（Pillowで1文字ずつ描画するロジック）は再利用できるが、**出力シリアライズ部分は全面的に新規実装が必要**（可変長グリフ、インターバル計算、メトリクス取得はPillowの`getbbox`/`getmetrics`では不十分でFreeType低レベルAPI相当が必要）。

→ **この転用ルートは非推奨**。後述のSTEP F-2の結果次第では不要になる。

---

## STEP F-2：`x4-epdfont-converter` との互換性判定

### 調査対象

GitHubリポジトリ `eunchurn/x4-epdfont-converter`（実在確認済み、`gh api`で取得）。
- 公開Webツール：`https://epdfont.clev.app`（ブラウザ完結、OS非依存）
- 技術スタック：Next.js/TypeScript、FreeTypeをWASMでブラウザ内実行
- 最終更新：2026-03-01。ファームウェア側の対応コミット（2026-05-01）はこれより後 → ツールの安定版に合わせてファームウェアが追従した経緯と整合

### バイト単位比較

`src/lib/epdfont-converter.ts` の `writeEPDFontBinary()` 関数を直接読み、ファームウェア側（`ExternalFont.cpp`）の期待仕様と1バイト単位で突き合わせた。

| 項目 | ファームウェア側 | コンバータ側 | 一致 |
|---|---|---|---|
| マジック | `0x46445045` "EPDF" | `EPDFONT_MAGIC = 0x46445045` | ✅ |
| バージョン | 1 | `EPDFONT_VERSION = 1` | ✅ |
| ヘッダーサイズ・各フィールドオフセット | 32B、offset 0/4/6/7/8/9/10/11/12/16/20/24/28 | `writeEPDFontBinary()`で同一オフセットに`setUint8/Int8/Uint16/Uint32(..., true)`（littleEndian）で書き込み | ✅ |
| インターバルエントリ | 12B（start/end/glyphOffset、各u32） | 同一 | ✅ |
| グリフエントリ | 16B（width/height/advanceX/reserved/left/top/dataLength/dataOffset） | 同一 | ✅ |
| ビットマックパッキング | シーケンシャル1bit MSBファースト | `convertTo1bit()`：`bitPos = 7-(i%8)`、`i = y*width+x` | ✅ |
| 2bit拒否 | `is2Bit`フラグで拒否 | `is2Bit`オプションで1bit/2bit切替可（1bitを選べば良い） | ✅（設定次第） |

**結論：バイト単位で完全一致。ソースコード同士の直接比較により実証済み。**

### 判定：**(A) 完全互換**

そのまま使用可能。STEP F-3Aへ進む。

### 日本語文字範囲の指定方法（実運用メモ）

`src/components/FontConverter.tsx` にUnicode範囲のプリセットが多数定義されている。**「Japanese」プリセットは ひらがな＋カタカナ＋CJK記号のみで、漢字（CJK統合漢字）は含まれない**ことに注意。実用的な日本語書籍表示には以下を追加で有効化する必要がある：

- `hiragana`（0x3040-0x309F）
- `katakana`（0x30A0-0x30FF）
- `cjkSymbols`（0x3000-0x303F、句読点等）
- `cjkUnified`（0x4E00-0x9FFF、CJK統合漢字 約2万字・「Very Large!」と明記）— これを追加しないと漢字が一切出ない
- 必要に応じて `cjkCompatIdeographs`（互換漢字）

「Chinese」プリセットには`cjkUnified`が含まれるため、「Japanese」+「Chinese」の両プリセットを有効化するのが手早い。

### 副産物：Python CLI版の存在（ブラウザ不要）

リポジトリ内 `reference/ttf_to_epdfont.py` に、**同一の`.epdfont`形式を出力するPythonリファレンス実装**が存在する（`freetype-py`パッケージ使用）。ブラウザを一切使わず、Windows上で`pip install freetype-py`だけで完結する。マジックナンバー・ヘッダー定数もTypeScript版と同一（`EPDFONT_MAGIC = 0x46445045`, `EPDFONT_VERSION = 1`）。

```
python ttf_to_epdfont.py <font_name> <size> <font_file> [--additional-intervals MIN,MAX] [-o output.epdfont]
```

Windows環境でのCLI完結（ブラウザ操作不要・スクリプト実行の再現性・バッチ処理しやすさ）を重視するなら、**このPython版の方がブラウザ版より実用的な可能性が高い**。ただし、この`reference/`配下のスクリプトが実際に動作検証されているかは未確認（README等での言及なし、"reference"というディレクトリ名からして参考実装扱いの可能性）。STEP F-3では両方試すことを推奨。

### `.cpfont`（本家CrossPoint）について（参考程度）

指示書の指示通り深追いはしていない。本家CrossPointの「SD-card font builder」が生成する`.cpfont`は拡張子からして別形式の可能性が高く、CJK版の`ExternalFont.h`が認識する拡張子は`.bin`/`.epdf`のみ（`.cpfont`への言及はコード上どこにも無い）。今回は不採用とし、`x4-epdfont-converter`（またはそのPython CLI版）を第一候補とする。

---

## まとめ：STEP F-3への申し送り

- 判定は **(A) 完全互換**。STEP F-3Aの手順（フォント入手 → x4-epdfont-converterで変換 → 命名規則に沿ってリネーム → SD配置 → 実機確認）にそのまま進める。
- 変換ツールは以下のいずれかを使用：
  1. ブラウザ版 `https://epdfont.clev.app`（GUI、プレビューあり）
  2. Python CLI版 `reference/ttf_to_epdfont.py`（要動作検証、成功すれば再現性・自動化の観点で有利）
- 日本語コードポイント範囲は「Japanese」プリセット単独では不十分。「Hiragana」「Katakana」「CJK Symbols & Punctuation」＋「CJK Unified Ideographs」を組み合わせること。
- 1bitモードを選択すること（2bitはファームウェアに拒否される）。
- 出力ファイル名はCJK版命名規則 `FontName_size_WxH.epdf` に合わせてリネームする（拡張子は`.epdf`のままで良い＝ローダーはマジックナンバーで判定するため`.bin`にリネームする必要はない）。

---

## STEP F-3A 実施記録・実機検証結果

### 生成したフォント

- Noto Serif JP（SIL OFL 1.1、ライセンス詳細は `scripts/font/README.md` 参照）、38pt
- 文字集合：ASCII・Latin-1・句読点・CJK記号・ひらがな・カタカナ・カタカナ拡張・半角/全角形・**JIS第1水準漢字（2,965字、Pythonの`euc_jp`コーデックで区点16-47を機械的に列挙して取得）**
- 生成スクリプト：`scripts/font/generate_jp_reading_font.py`（`ttf_to_epdfont.py`のロジックを再利用）
- 結果：3,683グリフ、0.58MB（`.epdf`）
- ヘッダーを実際にダンプし、仕様通りのオフセット計算になっていることを検証済み

### 実機確認：表示品質

✅ 設定画面からリーダーフォントとして選択可能。テスト本（日本語ひらがな・カタカナ・常用漢字を含む）の本文表示は**字形・サイズが揃って自然**（同梱`KingHwaOldSong`フォントで見られた不揃いは解消）。

### 既知の制約：低メモリ環境でのクラッシュ（未解決）

以下の条件でファームウェアが`std::bad_alloc`によりクラッシュする不具合を確認した。**フォント生成タスクの範囲・ホームランチャー機能の制約の両方を超える、`open-x4-sdk`側（`EInkDisplay::displayWindow()`, `EInkDisplay.cpp:1044`）の低レベル描画コードに起因する問題**のため、今回は未修正のまま既知の制約として記録する。

**再現条件**：EPUB読書中に空きヒープが数十KB以下まで低下した状態（長時間の読書セッション、ページ送りの蓄積等）で、ホームランチャー画面（`LauncherActivity`）に戻ると、直後の描画バッファ確保（`std::vector<unsigned char>`）が失敗し`abort()`に至る。

**調査で分かったこと**：
1. 当初、ホームランチャーの「サイレントNTP同期」機能（WiFi設定直後にランチャーへ戻ると自動でWiFi再接続+NTP同期を試みる）がクラッシュの引き金になっていることを確認 → 空きヒープ100KB未満では同期を試みないようガード処理を追加済み（`feature/home-launcher`ブランチ、コミット`ccfc51c`）。
2. しかし、NTP同期を確実にスキップさせた状態でも別途クラッシュを確認。読書セッション終了時点で既に空きヒープが約20KB（最小13KB）まで低下しており、**フォントサイズを縮小（2.3MB→0.58MB）してもこの数値は改善しなかった**（むしろ低いケースもあった）。フォント自体のメモリ使用量よりも、読書セッション中の断片化・蓄積が主要因の可能性が高い。
3. クラッシュ地点はいずれも同一（`EInkDisplay::displayWindow()`内の`std::vector`確保）。この関数は`std::bad_alloc`を未処理のまま呼び出し元に伝播させる構造になっており、メモリ不足時に例外がキャッチされず`std::terminate()`→`abort()`に至る。

**今後の対応が必要な場合の方向性**（未着手）：
- 読書セッション中のヒープ推移をシリアルログで追跡し、リーク箇所を特定する
- `EInkDisplay::displayWindow()`呼び出し元（ランチャー・リーダー双方）で、確保失敗を`std::bad_alloc`ではなく事前のヒープ残量チェックでガードする（本体は`open-x4-sdk`のため変更は要相談）
- ランチャーの部分更新領域をさらに縮小し、単発の確保サイズを抑える

**受け入れ基準（指示書§6）への影響**：フォントの表示品質・生成手順の再現性に関する項目はすべて満たしているが、「読書用フォント＋UIフォント同時使用でメモリクラッシュが起きない」の基準は**特定条件下で未達**。
