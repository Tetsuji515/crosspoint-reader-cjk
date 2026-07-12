# ホーム時計エリアの時刻同期方式 — STEP C-1 調査メモ

対象指示書：`xteink_x4_clock_sync_instruction.md`
調査ブランチ：`feature/home-launcher`
調査日：2026-07-13

実装は未着手。以下は読み取り専用のコード調査・ESP-IDFヘッダー調査の結果のみ。C1-3のみ実機検証が別途必要（本メモでは検証コードの要件のみ記載）。

---

| # | 調査項目 | 該当ファイル・API/関数名 | 説明 | 判定への示唆 |
|---|---|---|---|---|
| C1-1 | ディープスリープ中のRTCタイマー動作継続 | `lib/hal/HalPowerManager.cpp:63-86`（`startDeepSleep()`） | **重要な発見**：このコードのコメントに明記されている通り、X4のスリープはESP32標準のディープスリープ（RTCドメインは通電維持）ではない。GPIO13がバッテリーラッチMOSFETに接続されており、スリープ時にこれをLOWにすることで**バッテリー動作時はMCU全体（RTCドメイン含む）が完全に電源断になる**（コード原文コメント：`Note that this means the MCU will be completely powered off during sleep, including RTC`）。復帰は電源ボタンがMOSFETを介して電源を再投入する、実質的な再起動。USB給電時のみ挙動が異なる可能性が示唆されている（`esp_deep_sleep_enable_gpio_wakeup`のコメント：`this is only useful for waking up on USB power`）。 | **判定Yに強く傾く**。RTCメモリ・RTCタイマーはバッテリー動作中のスリープを跨いで保持されない可能性が高い（C1-3の実機検証で最終確認要）。 |
| C1-2 | スリープ経過時間取得API | `esp_sleep_get_time_since_boot()` | インストール済みESP-IDF（`~/.platformio/packages/framework-arduinoespressif32-libs/esp32c3/include`）内を全文検索したが、**このAPIは存在しない**（このIDFバージョンでは未提供）。代替として`esp_timer_get_time()`（`esp_timer.h`、起動からのマイクロ秒を返す）はあるが、これも「起動（=RTC電源再投入）からの経過時間」であり、C1-1の発見（MCU完全電源断）を踏まえると、**スリープ中の実経過時間を計測する手段にはならない**（スリープ中はこのタイマー自体が止まる）。 | 判定X（RTCタイマーでの経過時間補正）の主要な実現手段が存在しない。判定Yを補強。 |
| C1-3 | `RTC_DATA_ATTR`のスリープ跨ぎ保持（要実機検証） | `esp_attr.h`（`#define RTC_DATA_ATTR _SECTION_ATTR_IMPL(".rtc.data", ...)`、コメント: "keep its value during a deep sleep / wake cycle"） | マクロ自体はESP-IDFの標準機能として存在し、`.rtc.data`セクションに変数を配置する。**ただしこの保証は標準的なESP32ディープスリープ（RTCドメイン通電維持）を前提**としており、C1-1で判明したX4固有のMOSFET完全電源断構成でも成立するかは文書からは判断できない。実機検証が必須。検証コードの要件：`RTC_DATA_ATTR uint32_t testCounter = 0;`をグローバルに宣言し、`setup()`冒頭で値をログ出力してからインクリメント・スリープに入る。スリープ→電源ボタンで復帰→ログでカウンタが保持されているか（0にリセットされていないか）を確認する。 | **未実施**。C1-1のハードウェア設計から判定Yに強く傾いているが、最終確定にはこの検証結果が必要（指示書の要求通り）。 |
| C1-4 | `syncTimeWithNTP()`の汎用性 | `src/util/NtpSync.h/.cpp` | 依存は`<esp_sntp.h>`とFreeRTOSのみ。WiFi接続関連のヘッダーやActivity固有の状態への依存は一切なく、**完全に独立した自由関数**。呼び出し元がWiFi接続済みであることだけが前提（コード内でWiFi接続処理は行わない）。`main.cpp`・`LauncherActivity`・任意のActivityから同一の呼び出し方でそのまま利用可能。 | C1-4は「再利用可能」で確定。追加の切り出し作業は不要。 |
| C1-5 | 「ついで同期」のフックポイント | `src/activities/network/WifiSelectionActivity.cpp` | `grep WiFi.begin(` の結果、実際の接続呼び出しは**`WifiSelectionActivity::attemptConnection()`（230/232行目）の1箇所のみ**（自分の`LauncherActivity.cpp`の呼び出しを除く）。KOReader同期・OPDSライブラリ閲覧・Calibreワイヤレス・ファイル転送など、WiFiを使う既存機能はすべて`startActivityForResult()`経由でこの`WifiSelectionActivity`を呼び出す共通導線になっている。接続成功の判定は`checkConnectionStatus()`内、`status == WL_CONNECTED`の直後（243〜256行目、`WIFI_STORE.setLastConnectedSsid(selectedSSID)`を呼んでいる箇所）に一本化されている。天気機能はまだ未実装（`AppRegistry.h`で`enabled=false`のプレースホルダのみ）。 | **理想的なフックポイントが1箇所に確定**：`WifiSelectionActivity::checkConnectionStatus()`のWL_CONNECTED分岐に`syncTimeWithNTP()`呼び出しを追加するだけで、既存の全WiFi機能に「ついで同期」を後付けできる。各機能側のコードは一切変更不要。 |

### 補足：スリープ移行フックの場所（C-2実装時用のメモ、今回は変更しない）

`src/main.cpp`の`enterDeepSleep()`（187〜198行目）が実際のスリープ移行処理。`APP_STATE.saveToFile()`（`CrossPointState`、190行目）で既にSDへの状態永続化を行っており、判定Yで時刻を保存する場合はこの直前後に1行追加するのが最小の変更になる。`CrossPointState.h`（`src/CrossPointState.h`）は単純な構造体＋バイナリ保存/復元の既存パターンを持っており、指示書が提案する独立した`/.apps/clock.json`を新設する代わりに、この`CrossPointState`に`lastKnownEpoch`/`lastNtpSyncEpoch`フィールドを追加して既存の保存機構に相乗りする案も検討の余地がある（C-2実装時に相談）。

起動時の時刻復元処理の挿入位置：`main.cpp`の`APP_STATE.loadFromFile()`（306行目付近）の直後、かつQuick Resumeの起動分岐（312〜324行目、A-10で確認済み）より前。指示書§4の制約と合致する。

---

## 最終判定：判定Y（SD永続化・折衷案）— 実機検証により確定

### C1-3 実機検証結果

検証コード（`main.cpp`の`setup()`冒頭、`RTCTEST`タグ、`RTC_DATA_ATTR static uint32_t rtcTestBootCount`をインクリメントしてから`powerManager.startDeepSleep(gpio)`を呼ぶ一時的なテストブロック）を実機で検証。

- 1回目起動（フラッシュ直後）：`RTC_DATA_ATTR boot count = 0` → インクリメント→スリープ
- **電源ボタンでの正規のスリープ→復帰を経た2回目起動：`RTC_DATA_ATTR boot count = 0`**（1ではなく0のまま。複数回の電源ボタン操作で再現）

→ **`RTC_DATA_ATTR`はこのデバイスのスリープサイクルを跨いで保持されない**ことを実機で確定的に確認した。C1-1で判明した「バッテリー動作時はMOSFETによりRTCドメインを含むMCU全体が完全に電源断になる」というハードウェア設計と完全に整合する結果。

### 確定した判定

**判定Y（SD永続化・折衷案）。** 判定Xの前提（RTCメモリでのスリープ経過時間補正）は成立しないことが実機で確認されたため、指示書STEP C-2Yの方針（スリープ移行直前にエポック時刻をSDへ保存し、起動時に読み出して即座に妥当な時刻を復元する）で実装を進める。

なお検証中、電源ボタンの通常押しでは復帰しない場面が1回あり、10秒以上の長押しで復帰した。単発の事象で再現性は未確認だが、C-2実装後の受け入れ基準確認（「スリープ復帰を数回繰り返してもクラッシュ・異常時刻が発生しない」）の際は長押しでの復帰も選択肢に入れておくとよい。

検証用の一時コード（`RTCTEST`ブロック）はC-2実装開始前に`main.cpp`から削除する。
