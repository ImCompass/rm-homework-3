# 电控组第三次培训作业

姓名：朱章铭
学号：20263008273

## 参考资料与驱动来源
- Keysking STM32 教程：外部中断、串口中断、数据包解析、IIC
- 软件 I²C OLED 驱动：第三次培训资料包（`OLED.c` / `OLED.h` / `OLED_Font.h`，基于江协科技 OLED 驱动）
- 部分代码由蓝色大肥鱼老师撰写，那些用结构体来实现数据包/数据包校验/全局或 `static`/`tx_busy` ， `feedback_pending`缓冲区这些我真的写不来，但是我真的review了，花了我好久好久好久:(

## 实现原理
1. 设置四个关键状态 run_state, direction, step_ms, led_index
2. 设置四个旗子 dir_key_flag pause_key_flag packet_ready tx_busy
3. 设置三个中断
- HAL_GPIO_EXTI_Callback 并且加了时间窗消抖 但是在抬起来的时候还是会触发（悲）没时间写更好的了，ai写的我也懒得review
- HAL_UART_RxCpltCallback
- HAL_UART_TxCpltCallback
4. 然后while起来 update_LEDs, Update_OLED!