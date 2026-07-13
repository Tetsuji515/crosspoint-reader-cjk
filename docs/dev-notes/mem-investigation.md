# 低メモリクラッシュ（std::bad_alloc）調査メモ

対象指示書：`xteink_x4_memory_investigation_instruction.md`
調査ブランチ：`feature/mem-investigation`
調査日：2026-07-13

---

## STEP M-1：計測環境

### 追加した計測ユーティリティ

`src/util/MemLog.h/.cpp`（`MemLog::log(phase)`）。1回の呼び出しで以下4指標をCSV形式1行で出力する。挙動は一切変えず、ログ出力のみ（指示書§4「計測フェーズでは挙動を変えない」を遵守）。

```
[MEM] <phase>,<free>,<minfree>,<largest>,<internal>
```

| 列 | ESP-IDF API | 意味 |
|---|---|---|
| free | `esp_get_free_heap_size()` | 空きヒープ総量 |
| minfree | `esp_get_minimum_free_heap_size()` | 起動来の最小空き |
| largest | `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)` | 最大連続空きブロック（断片化判定） |
| internal | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` | 内部DRAM空き |

### 計測ポイント（埋め込み済み）

| phase タグ | 場所 | 対応する指示書の計測ポイント |
|---|---|---|
| `boot_end` | `main.cpp` setup()末尾 | ①起動直後 |
| `launcher_onEnter` | `LauncherActivity::onEnter()` | ②ホーム表示時 |
| `launcher_onExit` | `LauncherActivity::onExit()` | ⑤遷移前後 |
| `epubreader_onEnter` | `EpubReaderActivity::onEnter()` | ③本を開いた直後 |
| `epubreader_page_rendered` | ページ描画・キャッシュクリア後 | ④ページ送り毎 |
| `epubreader_onExit` | `EpubReaderActivity::onExit()` | ⑤遷移前後 |
| `launcher_handleBack_opens_reader` | `LauncherActivity::handleBack()` のReader起動直前 | M1.5-1の決定的証拠（後述） |

加えて、既存の `main.cpp` loop()が10秒毎に出力する `[MEM] Free: ... ` ログ、および `[ACT] Entering/Exiting activity: X` の遷移ログもそのまま活用する。

### ログ採取手順（人間が実機で実施）

1. `feature/mem-investigation` ブランチのファームを書き込む（済／要書き込み）
2. `PYTHONIOENCODING=utf-8 PYTHONUTF8=1 python scripts/debugging_monitor.py` でシリアルログ監視
3. 以下をワンセットで操作し、ログを採取（指示書§6の効率的手順）：
   - ①通常の読書→ホーム復帰（クラッシュ再現狙い）
   - ②読書↔ホームを数往復
   - ③各アプリ（設定・ファイル転送）→「戻る」でホーム復帰し、**その直後に本が開くか**を観察

---

## STEP M-1.5：異常な画面遷移（ホーム復帰時のReader起動）★最優先

### 静的コード解析による所見（実機ログ採取前）

**原因(e)（ホーム復帰時に意図せずReaderが再起動）を、コード解析でほぼ特定した。** メカニズムは以下：

1. **アプリ側は「押した瞬間」に終了する**：`SettingsActivity`（`SettingsActivity.cpp:124` `wasPressed(Button::Back)` → `finish()`）と `CrossPointWebServerActivity`（ファイル転送、`wasPressed(Button::Back)`）は、Backボタンを**押した瞬間**にActivityを終了する。
2. **ランチャー側は「離した瞬間」にReaderを開く**：`LauncherActivity::handleBack()`（`wasReleased(Button::Back)` 契機）が、開いていた本があれば `activityManager.goToReader()` を呼ぶ。この`handleBack`は指示書のホーム開発指示書B-2「[戻る]でQuick Resume相当」に沿って実装したもの。
3. **1回のBack操作が遷移をまたいで二重発火する**：アプリでBackを押す→（押下エッジで）アプリ終了→`goHome()`で新しいランチャー生成→ユーザーはまだボタンを保持→指を離す→（離脱エッジが）新しいランチャーの`loop()`に届く→`handleBack()`発火→Readerが開く。
4. メインループ構造（`main.cpp` loop()：`gpio.update()` → `activityManager.loop()`）上、アプリの`finish()`→pop→`goHome()`→新ランチャー`onEnter()`は同一`activityManager.loop()`内で完了し、新ランチャーの`loop()`は次のメインループ反復から動く。押下エッジでアプリを抜けるため、物理ボタンは遷移中まだ保持されており、離脱エッジは新ランチャーが処理することになる。

### 決定的な傍証：コードベースに既存の防御機構がある

`EpubReaderActivity::loop()`（`EpubReaderActivity.cpp:118-128`）には `skipNextButtonCheck` という機構があり、コメントに **"Skip button processing after returning from subactivity"** と明記されている。サブActivityから戻った直後、ConfirmとBackの両ボタンが「押されておらず、かつ離脱エッジも無い」状態になるまでボタン処理をスキップする。

→ **このボタン漏れ問題はコードベースで既知であり、リーダーは対策済み。しかし新規実装した`LauncherActivity`には同じガードが無い**。これが観察2（アプリ→ホーム復帰で本が開く）の直接原因。修正パターンも既存コード（`skipNextButtonCheck`）として確立している。

### 判定（暫定・実機ログで最終確認）

**原因(e)が主因である可能性が極めて高い。** 対処は`open-x4-sdk`不要、CJK版側（`LauncherActivity`にボタンガードを追加、または`handleBack`の契機を`wasReleased`から見直し）で完結する見込み。

実機ログでの最終確認ポイント：
- アプリ→「戻る」の直後に `[MEM] launcher_handleBack_opens_reader` と `[ACT] Entering activity: EpubReader` が出るか（(e)の実証）
- その際の `epubreader_onEnter` 時点の `free`/`largest` の落ち込み（(e)のヒープ影響の定量化＝M1.5-3）

### 実機ログ（採取後に貼付）

_（人間による実機ログ採取待ち）_

---

## STEP M-2：仮説の切り分け

_（M-1.5の実機確認後、必要に応じて実施。(e)が主因と確定すれば、M-2は「(e)修正後にもクラッシュが残るか」の確認に軸足を移す）_

---

## STEP M-3：原因の確定と対策候補

_（M-1.5／M-2の結果を待って記載）_
