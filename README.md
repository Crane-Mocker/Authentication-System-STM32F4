# Authentication-System-STM32F4

## 功能描述

1. 口令以md5密文形式存储，计算checksum,双备份。
2. 用户输入三备份，计算checksum，自动更新，防止关键数据出错。
3. 实现us和ms粒度的delay，使用RNG硬件随机数,实现粒度随机、时间随机的时间随机化。放置side-channel attack.
4. usart parity even.
5. 超时判断
6. cannary防止溢出

## short-comings(to do)

1. HAL_GetTick() 计时器翻转
为unsigned int型，不会为负，要判断是否翻转
2. 通信按位接收
通信时应该1位位接收，这样就可以通过传输时间判断是否输完口令。口令在收到第一位之后，其他位应当连续收到，即根据baud率计算传输1位所需要的tick，超时则串口初始化，清空缓冲区。
当前每8位接收，若是只输入了例如3位，则初始化串口。这导致发送了3位时需要等待规定tick数才能给出错误提示，开始下一次输入。
