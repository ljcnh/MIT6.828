这一章 做的稀里糊涂的...

说难吧 也没有那么难；说简单吧，不知道在干啥
可能是我直接做lab的原因，其他都没看。。。

按照提示一步一步来

```c
int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //

  // 提示中没有提高锁
  // 一开始没写  然后多次重复测试后 有概率失败。。。
  // 有一个测试是 multi-process多进程测试。。。
  // 需要保证并发安全

  acquire(&e1000_lock);

  uint32 tdt = regs[E1000_TDT]; // 通过E1000_TDT获取下一个数据包的 TX 环索引

  struct tx_desc *tx = &tx_ring[tdt]; // 获取对应索引处的数据

  if((tx->status & E1000_TXD_STAT_DD) == 0) {  // 检查环是否溢出
    release(&e1000_lock);
    return -1;
  }
  
  if (tx_mbufs[tdt]) {  // 使用 mbuffree() 释放从该描述符传输的最后一个 mbuf（如果有的话）
    mbuffree(tx_mbufs[tdt]);
  }

  // 填写描述符
  // m->head 指向数据包在内存中的内容
  // m->len 是数据包的长度
  // 设置必要的 cmd 标志（3.3） 第 37 页
  tx->addr = (uint64)m->head;
  tx->length = m->len;
  tx->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  // 在对应位置上设置 mbuf（记录指针）
  tx_mbufs[tdt] = m;
  
  // 将 E1000_TDT 模 TX_RING_SIZE 加一来更新环位置
  regs[E1000_TDT] = (tdt + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  
  // 这个没有用锁
  // e1000_recv 只会被 e1000_intr 调用
  // e1000_intr 只会在trap的时候被才可能会被使用
  // 应该没有啥并发 （运行了几十次 都没报错 应该没啥问题...）

  // 获取 E1000_RDT 控制寄存器并添加一个模 RX_RING_SIZE，向 E1000 询问下一个等待接收的数据包（如果有）所在的环索引
  uint32 rdt = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  struct rx_desc *rx = &rx_ring[rdt];

  // 此处是while  一次中断的时候 要recv所有满足条件的
  // 检查描述符状态部分中的 E1000_RXD_STAT_DD 位来检查是否有新数据包可用
  while(rx->status & E1000_RXD_STAT_DD) {
    
    // 将 mbuf 的 m->len 更新为 descriptor 中报告的长度
    rx_mbufs[rdt]->len = rx->length;
    // 使用 net_rx() 将 mbuf 传送到网络堆栈
    net_rx(rx_mbufs[rdt]);

    // 使用 mbufalloc() 分配一个新的 mbuf 来替换刚刚给 net_rx() 的那个
    rx_mbufs[rdt] = mbufalloc(0);
    // 将其数据指针 (m->head) 记录到 descriptor 中
    rx->addr = (uint64)rx_mbufs[rdt]->head;
    // 将描述符的状态位清零
    rx->status = 0;
    
    // 循环处理
    // 获取下一个要处理的
    // 更新rdt 同时更新对应的rx
    rdt = (rdt + 1) % RX_RING_SIZE;
    rx = &rx_ring[rdt];
  }
  
  // 将 E1000_RDT 寄存器更新为最后处理的环描述符的索引
  // 为啥要 -1 ？
  // 和前面是对应的  在开始的时候(66行)我们获得的是 (regs[E1000_RDT] + 1) % RX_RING_SIZE
  //    在while循环中一直 (rdt + 1) % RX_RING_SIZE 
  //    regs[E1000_RDT]  当前所指向的应该是符合条件的那个
  regs[E1000_RDT] = (rdt- 1) % RX_RING_SIZE;
}
```
