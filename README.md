# XIAO Reflex

A tiny reaction-time game for the **Seeed Studio XIAO nRF52840 Plus 1.47-inch touch display board**. Wait for the screen to turn green, then tap anywhere as quickly as you can.

## How to play

1. Tap the screen to start a round.
2. Wait through the amber countdown. The delay is randomized.
3. Tap anywhere as soon as the full screen turns green.
4. See your reaction time, rank, round number, and best time this session.

Touching before green is a false start. Tap again to retry. The best time is kept in RAM and resets when the board restarts.

## Hardware

- Seeed Studio XIAO nRF52840 Plus 1.47-inch touch display board
- 172 × 320 JD9853A LCD
- AXS5106L touch controller

No extra parts are needed. The touch controller's active-low interrupt on **D7** timestamps taps while the LCD is busy drawing the green cue; normal touch polling is kept as a fallback.

## Build and flash

This sketch was built and uploaded to the board with Arduino CLI. To build it yourself:

1. Install the **Seeed nRF52 Boards** Arduino core and select `Seeed XIAO nRF52840 Plus` (`Seeeduino:nrf52:xiaonRF52840Plus`).
2. Install **Seeed_GFX2** by Seeed Studio.
3. Open `ReactionTap147.ino` from this folder in Arduino IDE, or run:

   ```sh
   arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840Plus .
   arduino-cli upload --fqbn Seeeduino:nrf52:xiaonRF52840Plus --port COM_PORT .
   ```

   Replace `COM_PORT` with the board's port (for example, `COM39` on Windows).

The display and touch setup uses the JD9853A and AXS5106L drivers provided by Seeed_GFX2.

## Timing note

Timing begins immediately before the green-screen transition is drawn. The touch interrupt records the first tap during the LCD transfer, avoiding the extra delay that screen rendering would introduce into the reaction measurement. The result is intended as a fun personal challenge, not a calibrated scientific measurement.

## Interface

The game uses a dark cyan technical-style interface for the ready, waiting, and result screens, a full-screen green cue, and a red false-start screen. The green full-screen moment is the central visual cue.

## 中文简介

这是一个运行在 Seeed Studio XIAO nRF52840 Plus 1.47 英寸触摸屏开发板上的反应速度小游戏：点击开始，等待随机倒计时；屏幕变绿后尽快点击屏幕任意位置。抢先触摸会判定为过早按下。结果页显示反应时间、评级、本轮编号和本次开机以来的最佳成绩。

程序通过触摸控制器 D7 中断记录触摸时刻，避免 LCD 绘制绿色提示时漏掉点击。最佳成绩仅保存在 RAM 中，重启后清零。
