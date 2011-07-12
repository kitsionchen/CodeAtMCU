/*****************************************************************************
红外线遥控器按键检测程序.
通过检测两次下降沿之间的时间差判断接受到的数据位.
具有数据重发功能,但是数据的重发有点频繁.
使用晶震频率为11.0592MHz,所有定时值都是基于这个频率计算.
占用很少量的CPU时间,使用了外部中断0接受数据,定时器1进行计数,
状态机:
    1.如果时间差=0,由空闲态进入接受态
    2.如果时间差>1ms and <1.3ms,收到数据0
    3.如果时间差>2ms and <2.5ms,收到数据1
    4.如果时间差>13.2ms and <13.8ms,收到开始位
    5.如果时间差>12.2ms and <12.8ms,收到停止位(没有检测)
    6.如果时间定时器溢出(时间差>20ms),进入空闲状态
IR6222遥控芯片接收程序：
占用以下资源：
1. 遥控使用外部中断0，接P3.2口
2. 遥控使用定时计数器1
 ****************************************************************************/
#include "infraredray.h"

#define TIME_0_00MS    0x0000
#define TIME_1_00MS    0x0399
#define TIME_1_13MS    0x0469
#define TIME_2_00MS    0x07cf
#define TIME_2_50MS    0x09c3
#define TIME_13_2MS    0x338f
#define TIME_20_0MS    0x4e1f
#define TIME_9_00MS    0x2066
#define TIME_12_3MS    0x300c
#define TIME_14_0MS    0x3266
#define TIME_15_0MS    0x3a98
#define TIME1_LOAD     (0xffff - TIME_20_0MS)

// 红外接收的状态码
#define IR_NOSIGNAL        0x00    //无红外信号状态
#define IR_HEADCHK         0x01    //红外接收头，引导码状态
#define IR_DATACHK         0x02    //接收数据状态
#define IR_RECEIVE_OK      0x03    //接收完成32位状态
#define IR_RECEIVE_ERROR   0x04    //接收错误状态

uchar irok;
uchar irdata;
uchar ircheck = 0;
uchar ir_bit_cnt = 0;
unsigned long irbuf = 0;

/*****************************************************************************
功能：红外接收程序初始化
返回：返回32位红外码，包含系统码
 ****************************************************************************/
void irInit(void) {
  IP |= 0x01;
  IE |= 0x83;
  TCON |= 0x11;
  TMOD |= 0x01;
  TH0 = TIME1_LOAD >> 8;
  TL0 = TIME1_LOAD & 0xff;
}

/*****************************************************************************
功能：检测红外码
返回：1：有红外信号，0：无红外信号
****************************************************************************/
uchar irTest(void) {
  uchar ok = irok;
  irok = 0;
  return ok;
}

/*****************************************************************************
功能：获取8位红外数据码
返回：返回红外数据码
 ****************************************************************************/
uchar irGetDataCode(void) {
  irok = 0;
  return irdata;
}

/*****************************************************************************
功能：获取32位红外码，包含系统码
返回：返回32位红外码，顺序：16位系统码，8位数据码，8位数据反码
 ****************************************************************************/
#if GET_CODE32
unsigned long irGetAllCode(void) {
  irok = 0;
  return irbuf;
}
#endif

/*****************************************************************************
功能：红外解码程序（外中断服务程序）
描述：遥控使用外部中断0，接P3.2口
 ****************************************************************************/
void int0Isr(void) interrupt 0 {
  unsigned int timer;
  EX0 = 0;    // 关外中断，防止错误

  if (ircheck == IR_NOSIGNAL) {    // 引导脉冲第一次引起中断，把计数器清除
    TR0 = 0;    // 停止计数器
    TH0 = TIME1_LOAD >> 8;    //重赋定时初值
    TL0 = TIME1_LOAD & 0xff;  //重赋定时初值
    TR0 = 1;    // 开启计数器
    ircheck = IR_HEADCHK;    //状态切换到数据码检测
  } else {    // 如果不是引导脉冲第一次引起中断，则把计数器值读出来供下面比较
    TR0 = 0;
    timer = ((TH0 << 8) | TL0) - TIME1_LOAD;
    TH0 = TIME1_LOAD >> 8;
    TL0 = TIME1_LOAD & 0xff;
    TR0 = 1;
  }

  if (ircheck == IR_HEADCHK) {    //判断引导脉冲9+4.5=13.5ms 
    if (timer > TIME_12_3MS && timer < TIME_15_0MS) {
      ircheck = IR_DATACHK;
    }
    ir_bit_cnt = 0;
  } else if (ircheck == IR_DATACHK) {
    if (timer > TIME_1_00MS && timer < TIME_1_13MS) {    //接收的数据位0
      ir_bit_cnt++;
      irbuf = (irbuf << 1);    //把数据装入irbuf
      if (ir_bit_cnt == 32) {
	  	ircheck = IR_NOSIGNAL;    //回到无状态
      }
 	} else if (timer > TIME_2_00MS && timer < TIME_2_50MS) {    //接收数据位1
      ir_bit_cnt++;
      irbuf = (irbuf << 1) | 1;
      if (ir_bit_cnt == 32) {
        ircheck = IR_NOSIGNAL;
      }
    }
  }
  EX0 = 1;
}

/*****************************************************************************
功能：定时计数，红外码校验程序（定时器1中断服务程序）
 ****************************************************************************/
void time0Isr(void) interrupt 1 {
  TH0 = TIME1_LOAD >> 8;
  TL0 = TIME1_LOAD & 0xff;

  //校验数据码和数据反码
  if (((irbuf >> 8) & 0xff) == ((~(irbuf >> 0)) & 0xff)) {
    irdata = (irbuf >> 8) & 0xff;
    irok = 1;
  }

  TR0 = 0;
}

