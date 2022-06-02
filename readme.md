# Sensor Server
## 简介
Sensor Server： 这是一个用 C 实现的传感器数据采集 TCP Based 服务器，适合运行在低级Linux 设备上面。

## 协议规范
### 报文格式
数据报文格式非常简单，类型1字节，长度2字节，数据N字节：
    <img src="./doc/data-struct.png"></img>

### 报文列表

| 类型 | 名称         | 说明                         |
| ---- | ------------ | ---------------------------- |
| 0x00 | PING         | 心跳 PING                    |
| 0x01 | PING_OK      | 心跳回包: 0, 超时时间5s      |
| 0x02 | CONN         | 请求连接 CONN                |
| 0x03 | CONN_ACK     | 连接回复: 0x0-成功；0x1-失败 |
| 0x04 | DIS_CONN     | 请求断开 CONN                |
| 0x05 | DIS_CONN_ACK | 断开回复: 0x0-成功；0x1-失败 |
| 0x06 | SEND         | 发送数据                     |
| 0x07 | SEND_ACK     | 发送回复: 0x0-成功；0x1-失败 |
| 0x08 | PUBLISH      | 发布数据:                    |
| 0x09 | PUBLISH_ACK  | 发布回复: 0x0-成功；0x1-失败 |

### 报文说明

#### 0x00 | PING

#### 0x01 | PING_OK

#### 0x02 | CONN

#### 0x03 | CONN_ACK

#### 0x04 | DIS_CONN

#### 0x05 | DIS_CONN_ACK

#### 0x06 | SEND

#### 0x07 | SEND_ACK

#### 0x08 | PUBLISH

#### 0x09 | PUBLISH_ACK
