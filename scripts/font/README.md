# 日本語読書用フォント（.epdf）生成

`docs/dev-notes/reading-font-format-survey.md` の調査結果に基づき、CJK版ファームウェアが読み込める `.epdf`（EPDFont形式）の読書用フォントを、Windows環境のまま（DotInk不要）で生成する手順とスクリプト。

## 構成

- `ttf_to_epdfont.py` — [eunchurn/x4-epdfont-converter](https://github.com/eunchurn/x4-epdfont-converter) の `reference/ttf_to_epdfont.py` をそのまま取得したもの（ファームウェアの `ExternalFont` ローダーとバイト単位で互換であることを確認済み）。
- `generate_jp_reading_font.py` — 上記の内部ロジックを再利用しつつ、日本語の読書に実用的なUnicode範囲（ASCII、Latin-1、句読点、CJK記号、ひらがな、カタカナ、カタカナ拡張、CJK統合漢字、CJK互換漢字、半角/全角形）だけに絞って生成するラッパー。`ttf_to_epdfont.py` のデフォルト文字集合はハングル・絵文字・CJK拡張A〜Fまで含み不要に肥大化するため、実用最小限に絞っている。
- `test_small_epdfont.py` — 少数文字（「あいうテスト用電子書籍」＋ASCII）だけで生成し、フォーマット検証に使った小規模テスト用スクリプト。

## 使用フォント

**Noto Serif JP**（Google Fonts、`google/fonts` リポジトリの `ofl/notoserifjp/NotoSerifJP[wght].ttf` から取得）

- ライセンス: **SIL Open Font License 1.1**（`https://raw.githubusercontent.com/google/fonts/main/ofl/notoserifjp/OFL.txt` を参照。ビットマップ化した派生物の再配布も許諾条件内で可能）
- 著作権表記: Copyright 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name 'Source'. Copyright 2021 The Noto Project Authors (github.com/notofonts/noto-cjk)（OFL.txt本文より）
- 本リポジトリにはフォント原本（.ttf）・生成物（.epdf）はコミットしていない（`.gitignore` の `scripts/font/assets/` 参照）。容量とライセンス表記の煩雑さを避けるため、生成スクリプトのみを管理し、必要な人が同じ手順で再生成する運用とした。

## 再現手順

```bash
# 1. 依存パッケージ
python -m pip install freetype-py

# 2. フォント取得（例）
mkdir -p scripts/font/assets
curl -sL "https://raw.githubusercontent.com/google/fonts/main/ofl/notoserifjp/NotoSerifJP%5Bwght%5D.ttf" \
  -o scripts/font/assets/NotoSerifJP-Regular.ttf

# 3. 生成（38pt、実機のPYTHONUTF8対応込み）
PYTHONIOENCODING=utf-8 PYTHONUTF8=1 python scripts/font/generate_jp_reading_font.py \
  scripts/font/assets/NotoSerifJP-Regular.ttf 38 \
  scripts/font/assets/NotoSerifJP-Regular_38_38x42.epdf

# 4. SDカードの /fonts/ に配置し、設定画面のリーダーフォントで選択
```

## フォーマットの互換性について

`.epdf` はマジックナンバー `"EPDF"` を含むヘッダー（32バイト）＋インターバル配列＋グリフメタデータ配列＋ビットマップblobの構成。ファームウェア側 (`lib/ExternalFont/ExternalFont.cpp`) とのバイト単位一致は `docs/dev-notes/reading-font-format-survey.md` に検証済み。生成物のヘッダーも実際にダンプして仕様と突き合わせ済み（本ディレクトリのコミット履歴・作業ログ参照）。
