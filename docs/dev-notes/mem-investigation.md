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

### 実機ログ（2026-07-13 採取・(e)実証）

設定アプリ→「戻る」でホーム復帰した際のログ抜粋：

```
[13:26:41] [ACT] Exiting activity: Settings
[13:26:41] [ACT] Entering activity: Launcher
[13:26:41] [MEM] launcher_onEnter,79904,20388,49140,79904
[13:26:41] [MEM] launcher_handleBack_opens_reader,path=/Git GitHub入門レポート.md   ← (e)発火
[13:26:42] [MEM] launcher_onExit,79672,20388,49140,79672
[13:26:42] [ACT] Exiting activity: Launcher
[13:26:42] [ACT] Entering activity: Reader → TxtReader                            ← 本が再オープン
```

**設定から戻った同一秒のうちに `launcher_handleBack_opens_reader` が発火し、意図せず本が再オープンされている。原因(e)を実機ログで確定。** ユーザーの目視でも「ホームに帰った直後に本が意図せず開いた」ことを確認済み。

なお本セッションで再オープンされたのは`.md`ファイル（軽量な`TxtReader`）だったため、**クラッシュには至らなかった**。クラッシュは、再オープン対象が重いEPUB＋日本語フォント（大きなグリフ展開を伴う）の場合に、その再ロードのピークが利用可能な連続メモリを超えて発生する（後述M-2判定2）。

---

## STEP M-2：仮説の切り分け（実機ログに基づく）

計測ログ（`launcher_onEnter/onExit` のヒープ4指標推移）：

| タイミング | free | minfree | largest | 備考 |
|---|---|---|---|---|
| `boot_end` | 117,316 | 116,204 | 86,004 | 起動直後 |
| 初回 launcher_onEnter | 117,444 | 116,204 | 86,004 | 読書前 |
| 初回読書後 launcher_onExit | 80,056 | 79,336 | 49,140 | 読書で約37KB恒常消費 |
| 以降 launcher_onEnter ×8往復 | **79,904（一定）** | 20,388 | **49,140（一定）** | 往復で不変 |

### 判定1：単調増加か（仮説a/c：キャッシュ/リーク増大）→ **否定**

読書↔ホームを8往復しても、ホーム表示時の `free` は **79,904 で完全に一定**。往復ごとに下がる現象は無い。**リーク／解放漏れ（仮説c）も、セッション内キャッシュの往復蓄積（仮説a）も、往復レベルでは発生していない。** ただし、初回読書で `free` が 117KB→80KB へ約37KB恒常減少し、以後回復しない。これは外部フォントのグリフキャッシュ／リーダー関連の常駐分と推測され、往復では増えない「一度きりの階段」であり単調増加ではない。

### 判定2：断片化か（仮説b）→ **該当（クラッシュの近接要因）**

ホーム復帰時、`free`（総量）は約79,904（78KB）あるのに、`largest`（最大連続ブロック）は **49,140（48KB）に留まる**。約30KBが断片化して非連続。`displayWindow()` や重いEPUB＋フォントの再ロードが48KBを超える連続領域を要求すると、**総量は足りていても`bad_alloc`になる**。過去のクラッシュ（`min_free_heap`≈3〜13KB、`displayWindow`内の`std::vector`確保失敗）はこの断片化＋(e)による再ロードピークの合わせ技。

### 判定3：遷移時ピークか（仮説d）→ **(e)の別表現として該当**

(e)により、ホーム復帰の直後にリーダーが再起動し、EPUB＋グリフ展開が再度走る。これが「遷移直後の瞬間的ヒープピーク」の実体。`ActivityManager::replaceActivity()` は旧Activity解放→新Activity生成の順（`loop()`内で`exitActivity`後に`currentActivity`差し替え）だが、(e)の再ロードは正規のホーム表示が終わる前に別の重い確保を挟むため、`minfree`≈20KB／`largest`≈48KBの状況で重いEPUBを再ロードすると容易に枯渇する。

### 判定4：フォントの寄与度

本セッションは`.md`（`TxtReader`）主体でEPUB＋日本語フォントの読書ログは未採取のため、フォント常駐サイズの定量化は追加採取が必要。ただし判定1が示す通り「初回読書で約37KB恒常消費し以後一定」であり、フォントはファイルサイズ（0.58MB）ではなくこの恒常分としてRAMを消費している。断片化後の`largest`≈48KBに対し、重いEPUB再ロードが必要とする連続領域がこれを超えることがクラッシュの本質。

---

## STEP M-3：原因の確定と対策候補

### 確定した原因

**主因：(e) ホーム復帰時に意図せずReaderが再起動する異常遷移（実機ログで確定）。**
- Settings/ファイル転送は Back **押下**（`wasPressed`）で終了、`LauncherActivity::handleBack()` は Back **離脱**（`wasReleased`）でReaderを開く。1回のBack操作が遷移をまたいで二重発火し、本が再オープンされる。
- **近接的な失敗要因：(b) 断片化。** ホーム復帰時 `free`≈78KB に対し `largest`≈48KB。(e)による重いEPUB再ロードが48KBを超える連続確保を要求し `displayWindow()` で `bad_alloc`。
- (a)(c) 往復リーク／単調増加は**否定**（`free`が往復で一定）。

### 対策候補

**A. CJK版の範囲内で対応可能（推奨・SDK変更不要）**

1. **【主対策】`LauncherActivity` にボタン漏れガードを追加**：`EpubReaderActivity::loop()` の `skipNextButtonCheck`（`EpubReaderActivity.cpp:118-128`、サブActivityから戻った直後、Back/Confirmが「押されておらず離脱エッジも無い」状態になるまでボタン処理をスキップ）と同じパターンを`LauncherActivity`にも実装する。**コードベースに既存の対策パターンがあり、それを踏襲するだけ**。これで(e)（＝クラッシュの主トリガ）が解消する見込み。
   - 効果：大（クラッシュの主因を断つ）。副作用：ほぼ無し（遷移直後の1フレームだけボタンを無視するのみ）。
2. （補助）`handleBack()` の契機を `wasReleased` から `wasPressed` に変更し、アプリ側と揃える案もあるが、これ単体では「押下が遷移前のアプリに、離脱が新ランチャーに」という別パターンの漏れを完全には塞げないため、1のガード方式が本命。

**B. `open-x4-sdk` 側の変更を要する（要相談・当面は着手しない）**

3. **断片化(b)への対策**：`EInkDisplay::displayWindow()`（`open-x4-sdk`）が要求する連続バッファのサイズ調査と、確保方法の見直し（部分更新の分割、事前確保の再利用等）。これはSDK側の変更を伴うため要相談。**ただし主対策A-1で(e)を断てば、通常運用でのクラッシュは解消する見込みが高く、Bは優先度低。**

### 推奨

**まず対策A-1（ランチャーのボタン漏れガード）のみを実装し、クラッシュが再現しなくなるか実機確認する。** これで解消すれば断片化(b)への踏み込み（SDK変更）は不要。実装可否・実装は人間の判断を仰ぐ（本調査では実装まで進めない）。
