# -*- coding: utf-8 -*-
"""
WHSD 控制板协议模拟服务端
================================
模拟绝缘子零值检测机器人下位机（控制板），通过 TCP 与上位机软件通讯。

协议参考: Insulator_Zero_Value_Detection_Robot/Protocol/WHSDControlBoradProtocol.cpp

帧格式:
    FF FE | len(1B) | packNum(1B) | cmd(1B) | data... | checksum(1B) | FD FC
    - len: 整帧长度(含包头包尾)
    - checksum: 从 FF 到 data 末尾(即前 len-3 字节)的累加和(低8位)

上位机(客户端)行为要点:
    - 周期发送心跳 0x00(1字节: 最近收到的包序号) -> 设备须回 0x00(64字节状态)
    - 发送测量结果 0x0F 后, 上位机会回 ACK: 0x0F {0x01}
    - 发送版本查询 0x06 后, 上位机会回 ACK: 0x06 {0x01}

设备状态(心跳 64 字节)布局:
    [0..3]   行走电机(8个): byte0=使能位图, byte1..3=每电机3位状态
    [4..7]   卷扬电机
    [8..11]  安全电机
    [12..15] 采样板电机
    [16]     电池百分比
    [17..20] 固件 年/月/日/当日编译次数
    [21]     射线机状态(0空闲 1延时开启中 2工作完成)
    [22]     总电源(0关 1开)
    [23]     bit0 工厂模式

用法:
    python device_simulator.py [port]        # 默认 8234
连接后在控制台输入命令交互(输入 help 查看列表)。
"""

import socket
import struct
import threading
import time
import sys
import queue

# ---------------- 协议常量 ----------------
HEAD = b"\xff\xfe"
TAIL = b"\xfd\xfc"

CMD_HEARTBEAT = 0x00
CMD_DELAY_TIME = 0x01
CMD_PULSES = 0x02
CMD_XRAY_START = 0x03
CMD_XRAY_STOP = 0x04
CMD_DEVICE_RUN = 0x05
CMD_VERSION = 0x06
CMD_LOG = 0x07
CMD_POWER_ALL = 0x08
CMD_FACTORY_MODE = 0x09
CMD_OTA_ENTER = 0x0A
CMD_OTA_DATA = 0x0B
CMD_OTA_END = 0x0C
CMD_CONFIG = 0x0D
CMD_MEASURE_RESULT = 0x0F
CMD_SENSOR = 0x11
CMD_CALIB = 0x17

CALIB_RESET = 0x05
CALIB_QUERY_RAW = 0x06
CALIB_READ_COEF = 0x0C
CALIB_VERIFY = 0x0A
CALIB_SINGLE_POINT = 0x0E

# 心跳数据域固定长度
HEARTBEAT_DATA_LEN = 64
# 原始值有效时长(秒): 0x0F 回报后 5 秒内 0x06 才有效
RAW_VALUE_TIMEOUT = 5.0


def be32(n):
    """int32 大端(协议规定多字节整数一律大端)"""
    return struct.pack(">i", n)


def read_be32(b, off=0):
    return struct.unpack(">i", b[off:off + 4])[0]


class Frame:
    __slots__ = ("pack_num", "cmd", "data")

    def __init__(self, pack_num, cmd, data):
        self.pack_num = pack_num
        self.cmd = cmd
        self.data = data


def parse_stream(buf):
    """从字节流中解析完整帧, 返回 (帧列表, 剩余字节)"""
    frames = []
    while len(buf) > 8:
        if buf[0] != 0xFF or buf[1] != 0xFE:
            buf = buf[1:]
            continue
        pack_len = buf[2]
        if len(buf) < pack_len:
            break
        if buf[pack_len - 2] != 0xFD or buf[pack_len - 1] != 0xFC:
            buf = buf[1:]
            continue
        if (sum(buf[:pack_len - 3]) & 0xFF) != buf[pack_len - 3]:
            buf = buf[1:]
            continue
        frames.append(Frame(buf[3], buf[4], bytes(buf[5:pack_len - 3])))
        buf = buf[pack_len:]
    return frames, buf


class DeviceState:
    """模拟下位机的可变状态"""

    def __init__(self):
        self.lock = threading.Lock()
        # 电机: 4组 x (使能位图, 状态字) — 状态全 0 = 正常
        self.motor = [[0x00, 0x00, 0x00, 0x00] for _ in range(4)]
        self.battery = 88
        self.fw_year, self.fw_month, self.fw_day, self.fw_ver = 0x19, 0x09, 0x01, 0x03
        self.xray_status = 0x00
        self.main_power = 0x01
        self.factory_mode = False
        # 校准状态
        self.calib_a = 1000        # 毫值系数 a(乘到原始X上, 千分之一)
        self.calib_b = 0           # 毫值偏移 b
        self.last_raw_milli = None # 最近一次 0x0F 回报的原始值
        self.last_raw_time = 0.0
        # 传感器(测量模块)状态: 0待机 1触发 2触发完成 3未找到设备
        self.sensor_status = 0
        # 统计
        self.rx_count = 0
        self.tx_count = 0

    def heartbeat_bytes(self):
        with self.lock:
            d = bytearray(HEARTBEAT_DATA_LEN)
            for i in range(4):
                d[i * 4:i * 4 + 4] = bytes(self.motor[i])
            d[16] = self.battery
            d[17], d[18], d[19], d[20] = self.fw_year, self.fw_month, self.fw_day, self.fw_ver
            d[21] = self.xray_status
            d[22] = self.main_power
            if self.factory_mode:
                d[23] |= 0x01
            return bytes(d)


def cmd_name(cmd):
    return {
        CMD_HEARTBEAT: "心跳0x00", CMD_DELAY_TIME: "延时0x01", CMD_PULSES: "脉冲数0x02",
        CMD_XRAY_START: "开启射线0x03", CMD_XRAY_STOP: "停止射线0x04", CMD_DEVICE_RUN: "电机控制0x05",
        CMD_VERSION: "版本0x06", CMD_LOG: "设备日志0x07", CMD_POWER_ALL: "总电源0x08",
        CMD_FACTORY_MODE: "工厂模式0x09", CMD_OTA_ENTER: "OTA进入0x0A", CMD_OTA_DATA: "OTA数据0x0B",
        CMD_OTA_END: "OTA结束0x0C", CMD_CONFIG: "配置0x0D", CMD_MEASURE_RESULT: "测量结果0x0F",
        CMD_SENSOR: "传感器0x11", CMD_CALIB: "校准0x17",
    }.get(cmd, "未知0x%02X" % cmd)


def hexs(b):
    return " ".join("%02X" % c for c in b)


class DeviceServer:
    def __init__(self, port):
        self.port = port
        self.state = DeviceState()
        self.pack_num = 0
        self.sock = None          # 当前客户端连接
        self.write_lock = threading.Lock()
        self.send_queue = queue.Queue()  # 延迟应答队列 (delay_seconds, bytes)
        self.running = True
        # OTA 模拟
        self.ota_total = 0
        self.ota_received = set()

    # ---------------- 帧构造/发送 ----------------
    def build(self, cmd, data=b""):
        self.pack_num = (self.pack_num + 1) % 256
        # 帧长 = 包头5字节 + data + 校裁1 + 包尾2 = len(data) + 8 (与 C++ GetCmdData 一致)
        body = bytes([0xFF, 0xFE, len(data) + 8, self.pack_num, cmd]) + data
        chk = sum(body) & 0xFF
        return body + bytes([chk]) + TAIL

    def send(self, cmd, data=b"", delay=0.0):
        frame = self.build(cmd, data)
        if delay > 0:
            self.send_queue.put((delay, frame))
            return
        self._send_raw(frame)

    def _send_raw(self, frame):
        with self.write_lock:
            s = self.sock
            if s is None:
                return False
            try:
                s.sendall(frame)
                self.state.tx_count += 1
                return True
            except OSError:
                return False

    def sender_thread(self):
        """处理延迟应答(如 Flash 擦写 1~2 秒的定标应答)"""
        while self.running:
            try:
                delay, frame = self.send_queue.get(timeout=0.2)
            except queue.Empty:
                continue
            if delay > 0:
                time.sleep(delay)
            self._send_raw(frame)

    # ---------------- 命令处理 ----------------
    def handle(self, f: Frame):
        st = self.state
        st.rx_count += 1
        print("[RX] %s 序号=%d 共%d字节  %s" % (cmd_name(f.cmd), f.pack_num, len(f.data), hexs(f.data) if f.data else ""))

        if f.cmd == CMD_HEARTBEAT:
            # 上位机心跳: data=最近收到的设备包序号 -> 回 64 字节状态
            self.send(CMD_HEARTBEAT, st.heartbeat_bytes())
            return

        if f.cmd == CMD_VERSION:
            # 模拟固件版本回读(上位机会自动 ACK 0x06{0x01})
            self.send(CMD_VERSION, bytes([0x01]))
            return

        if f.cmd == CMD_DEVICE_RUN:
            target, enable, mode = f.data[0], f.data[1], f.data[2]
            with st.lock:
                if target < 4:
                    st.motor[target][0] = 0xFF if enable in (1, 2) else 0x00
            verb = {1: "运行", 2: "刹车", 3: "停止"}.get(enable, "未知(%d)" % enable)
            print("    >> 电机%d %s 模式=%d" % (target, verb, mode))
            # 模拟动作完成后回报日志
            self.send(CMD_LOG, ("[设备] 电机%d %s完成" % (target, verb)).encode("gbk", "replace"))
            return

        if f.cmd in (CMD_DELAY_TIME, CMD_PULSES, CMD_XRAY_START, CMD_XRAY_STOP,
                     CMD_POWER_ALL, CMD_FACTORY_MODE, CMD_CONFIG):
            if f.cmd == CMD_POWER_ALL:
                on = f.data[0] == 0x01
                with st.lock:
                    st.main_power = f.data[0]
                print("    >> 总电源 %s" % ("开" if on else "关"))
            elif f.cmd == CMD_XRAY_START:
                with st.lock:
                    st.xray_status = 0x01  # 延时开启中
                self.send(CMD_LOG, "[设备] 射线机开始延时开启".encode("gbk", "replace"))
                # 模拟延时结束 -> 工作完成
                self.send(CMD_LOG, "[设备] 射线机工作完成".encode("gbk", "replace"), delay=2.0)
                def _finish():
                    with st.lock:
                        st.xray_status = 0x02
                threading.Timer(2.0, _finish).start()
            elif f.cmd == CMD_XRAY_STOP:
                with st.lock:
                    st.xray_status = 0x00
            elif f.cmd == CMD_FACTORY_MODE:
                with st.lock:
                    st.factory_mode = f.data[0] == 0x01
            elif f.cmd == CMD_CONFIG:
                self.send(CMD_LOG, "[设备] 配置已写入".encode("gbk", "replace"))
            return

        if f.cmd == CMD_SENSOR:
            # 上位机: {index, cmd, value高, value低}; 设备回: {index, cmd, value(uint16 小端 memcpy)}
            index, scmd = f.data[0], f.data[1]
            value = 0
            if scmd == 2 or scmd == 3 or scmd == 4:
                value = st.sensor_status
            reply = bytes([index, scmd]) + struct.pack("<H", value)
            self.send(CMD_SENSOR, reply)
            return

        if f.cmd == CMD_CALIB:
            self.handle_calib(f.data)
            return

        if f.cmd == CMD_OTA_ENTER:
            # data[0]=0x00 请求进入; 回 0x02=进入且暂停心跳(上位机 0x02 时会暂停心跳)
            self.ota_total = 0
            self.ota_received = set()
            self.send(CMD_OTA_ENTER, bytes([0x02]))
            print("    >> 进入OTA模式(已请求上位机暂停心跳)")
            return

        if f.cmd == CMD_OTA_DATA:
            # data: 总包数(uint32 LE) + 当前包号(uint32 LE) + 长度(1B) + 数据 + checksum(1B)
            total, idx = struct.unpack("<II", f.data[0:8])
            self.ota_total = total
            self.ota_received.add(idx)
            print("    >> OTA 数据包 %d/%d" % (idx, total))
            # 回: {0x01} + 包序号(uint32 小端 memcpy)
            self.send(CMD_OTA_DATA, bytes([0x01]) + struct.pack("<I", idx))
            if idx == total:
                # 全部收完 -> 结束
                self.send(CMD_OTA_END, bytes([0x01]))
                print("    >> OTA 传输完成, 已发送结束指令")
            return

        if f.cmd == CMD_OTA_END:
            return

    def handle_calib(self, data):
        """0x17 校准命令模拟, 应答: 子命令+结果+原因+参数1(4B BE)+参数2(4B BE)"""
        st = self.state
        sub = data[0]

        def answer(result, reason=0, v1=0, v2=0, delay=0.0):
            payload = bytes([sub, result, reason]) + be32(v1) + be32(v2)
            self.send(CMD_CALIB, payload, delay=delay)

        if sub == CALIB_RESET:  # 恢复默认: 清空系数, 通道直通
            with st.lock:
                st.calib_a = 1000
                st.calib_b = 0
            answer(0x01, 0, st.calib_a, st.calib_b)
            print("    >> 校准已恢复默认 (a=1.000 b=0)")

        elif sub == CALIB_QUERY_RAW:  # 查询原始值: 只回参数1
            with st.lock:
                raw, t = st.last_raw_milli, st.last_raw_time
            if raw is None or (time.time() - t) > RAW_VALUE_TIMEOUT:
                # 超时: 结果00 原因01
                payload = bytes([sub, 0x00, 0x01]) + be32(0)
                self.send(CMD_CALIB, payload)
                print("    >> 查询原始值: 已超时(结果00 原因01)")
            else:
                payload = bytes([sub, 0x01, 0x00]) + be32(raw)
                self.send(CMD_CALIB, payload)
                print("    >> 查询原始值: %d 毫值" % raw)

        elif sub == CALIB_READ_COEF:  # 读回系数 a/b
            with st.lock:
                a, b = st.calib_a, st.calib_b
            answer(0x01, 0, a, b)
            print("    >> 读回系数: a=%d b=%d (毫值)" % (a, b))

        elif sub == CALIB_VERIFY:  # 补偿验证: 回显 原始值 + 校准后值
            if len(data) < 5:
                answer(0x00, 0x05)  # 参数长度错误
                return
            raw = read_be32(data, 1)
            with st.lock:
                a, b = st.calib_a, st.calib_b
            calibrated = raw * a // 1000 + b
            answer(0x01, 0, raw, calibrated)
            print("    >> 补偿验证: 原始=%d -> 校准后=%d 毫值" % (raw, calibrated))

        elif sub == CALIB_SINGLE_POINT:  # 单点定标: 写 Flash 1~2 秒
            if len(data) != 9:  # 子命令 + 8字节参数
                answer(0x00, 0x05)  # 参数长度/格式错误
                return
            std_milli = read_be32(data, 1)
            raw_milli = read_be32(data, 5)
            if std_milli <= 0 or raw_milli <= 0 or raw_milli >= std_milli:
                answer(0x00, 0x09)  # 参数非法(原始值必须小于标准值)
                return
            a = std_milli * 1000 // raw_milli
            b = 0
            with st.lock:
                st.calib_a, st.calib_b = a, b
            # Flash 擦写需 1~2 秒, 应答延迟 1.5 秒
            answer(0x01, 0, a, b, delay=1.5)
            print("    >> 单点定标: 标准=%d 原始=%d -> a=%d b=0 (1.5秒后应答)"
                  % (std_milli, raw_milli, a))
        else:
            answer(0x00, 0x05)

    # ---------------- 主动上报 ----------------
    def send_measure_result(self, mega_ohm, calibrated=True):
        """
        发送 0x0F 测量结果.
        帧数据: [0]=保留 [1]=类型(0=int32) [2..5]=流水号 [6..9]=X(int32 大端毫值)
        上位机收后会回 ACK 0x0F{0x01}.
        """
        st = self.state
        x_milli = int(round(mega_ohm * 1000))
        with st.lock:
            a, b = st.calib_a, st.calib_b
            st.last_raw_milli = x_milli
            st.last_raw_time = time.time()
            st.sensor_status = 2  # 触发完成
        out = x_milli if not calibrated else x_milli * a // 1000 + b
        data = bytes([0x00, 0x00]) + struct.pack(">I", st.tx_count & 0xFFFFFFFF) + be32(out)
        self.send(CMD_MEASURE_RESULT, data)
        print("[TX] 测量结果 0x0F: 原始=%s 输出=%.3f MΩ (a=%.3f b=%d)"
              % ("%.3f" % (x_milli / 1000.0) if calibrated else "直通",
                 out / 1000.0, a / 1000.0, b))

    def send_raw_result(self, mega_ohm):
        """发送未校准的原始值(通道直通)"""
        st = self.state
        x_milli = int(round(mega_ohm * 1000))
        with st.lock:
            st.last_raw_milli = x_milli
            st.last_raw_time = time.time()
        data = bytes([0x00, 0x00]) + struct.pack(">I", st.tx_count & 0xFFFFFFFF) + be32(x_milli)
        self.send(CMD_MEASURE_RESULT, data)
        print("[TX] 原始值 0x0F: %.3f MΩ (未校准直通)" % (x_milli / 1000.0))

    def send_sensor_status(self, status):
        """主动推送传感器状态(与轮询应答同格式)"""
        self.state.sensor_status = status
        data = bytes([0x00, 0x02]) + struct.pack("<H", status)
        self.send(CMD_SENSOR, data)
        print("[TX] 传感器状态 0x11: %d" % status)

    def send_log(self, text):
        payload = text.encode("gbk", "replace")
        self.send(CMD_LOG, payload)
        print("[TX] 设备日志 0x07: %s" % text)

    # ---------------- 连接处理 ----------------
    def client_loop(self, conn, addr):
        print("\n[连接] 上位机已接入 %s:%d" % addr)
        self.sock = conn
        buf = b""
        conn.settimeout(1.0)
        try:
            while self.running:
                try:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                    frames, buf = parse_stream(buf)
                    for f in frames:
                        try:
                            self.handle(f)
                        except Exception as e:
                            print("[错误] 处理 %s 失败: %s" % (cmd_name(f.cmd), e))
                except socket.timeout:
                    continue
                except OSError:
                    break
        finally:
            print("[断开] 上位机连接已断开")
            try:
                conn.close()
            except OSError:
                pass
            if self.sock is conn:
                self.sock = None

    def serve_forever(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("0.0.0.0", self.port))
        srv.listen(1)
        print("WHSD 控制板模拟服务端已启动: 0.0.0.0:%d" % self.port)
        print("等待上位机软件连接 (软件内系统设置中 IP 填本机地址, 端口 %d)..." % self.port)
        threading.Thread(target=self.sender_thread, daemon=True).start()
        while self.running:
            try:
                srv.settimeout(0.5)
                conn, addr = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            t = threading.Thread(target=self.client_loop, args=(conn, addr), daemon=True)
            t.start()
            t.join()  # 一次只服务一个上位机
        srv.close()

    # ---------------- 控制台 ----------------
    def console(self):
        help_text = """
可用命令:
  measure <MΩ>       发送测量结果(套用当前校准系数), 例: measure 520.5 / measure 0.8
  raw <MΩ>           发送未校准原始值(通道直通), 用于定标流程中连测原始值
  sensor <0-3>       设置/推送检测模块状态 (0待机 1触发 2触发完成 3未找到设备)
  log <文本>         发送一条设备日志(0x07)
  power on|off       切换总电源(心跳内上报)
  xray <0-2>         设置射线机状态
  battery <0-100>    设置电池电量
  motor <0-3> on|off 设置某组电机使能位图
  fault <0-3> <0-7>  设置某组电机故障码(每电机3位)
  calib              查看当前校准系数
  stat               查看收发包统计
  help               显示本帮助
  quit               退出模拟器
"""
        print(help_text)
        while self.running:
            try:
                line = input("sim> ").strip()
            except (EOFError, KeyboardInterrupt):
                # stdin 非交互(如被脚本拉起)时不退出, 保持服务运行
                try:
                    while self.running:
                        time.sleep(0.5)
                except KeyboardInterrupt:
                    break
                break
            if not line:
                continue
            parts = line.split()
            cmd = parts[0].lower()
            st = self.state
            try:
                if cmd == "quit" or cmd == "exit":
                    self.running = False
                    break
                elif cmd == "help":
                    print(help_text)
                elif cmd == "measure" and len(parts) >= 2:
                    self.send_measure_result(float(parts[1]))
                elif cmd == "raw" and len(parts) >= 2:
                    self.send_raw_result(float(parts[1]))
                elif cmd == "sensor" and len(parts) >= 2:
                    self.send_sensor_status(int(parts[1]))
                elif cmd == "log":
                    self.send_log(" ".join(parts[1:]) or "[设备] 空日志")
                elif cmd == "power" and len(parts) >= 2:
                    with st.lock:
                        st.main_power = 1 if parts[1].lower() == "on" else 0
                    print("总电源 -> %s" % ("开" if st.main_power else "关"))
                elif cmd == "xray" and len(parts) >= 2:
                    with st.lock:
                        st.xray_status = int(parts[1]) & 0xFF
                elif cmd == "battery" and len(parts) >= 2:
                    with st.lock:
                        st.battery = max(0, min(100, int(parts[1])))
                elif cmd == "motor" and len(parts) >= 3:
                    g = int(parts[1])
                    if 0 <= g < 4:
                        with st.lock:
                            st.motor[g][0] = 0xFF if parts[2].lower() == "on" else 0x00
                        print("电机组%d 使能位图 -> %s" % (g, hex(st.motor[g][0])))
                elif cmd == "fault" and len(parts) >= 3:
                    g, code = int(parts[1]), int(parts[2])
                    if 0 <= g < 4 and 0 <= code <= 7:
                        with st.lock:
                            # 8个电机统一置同一故障码(3位/电机)
                            v = 0
                            for i in range(8):
                                v |= (code & 0x07) << (i * 3)
                            st.motor[g][1:] = v.to_bytes(3, "little")
                        print("电机组%d 全部电机故障码 -> %d" % (g, code))
                elif cmd == "calib":
                    with st.lock:
                        print("当前校准系数: a=%.3f b=%d 毫值, 原始值=%s"
                              % (st.calib_a / 1000.0, st.calib_b,
                                 ("%d 毫值" % st.last_raw_milli) if st.last_raw_milli is not None else "无"))
                elif cmd == "stat":
                    print("接收帧: %d  发送帧: %d  当前连接: %s"
                          % (st.rx_count, st.tx_count, "在线" if self.sock else "离线"))
                else:
                    print("未知命令, 输入 help 查看用法")
            except ValueError:
                print("参数格式错误")
            except Exception as e:
                print("命令执行失败: %s" % e)
        self.running = False
        print("控制台已退出")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8234
    server = DeviceServer(port)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    try:
        server.console()
    finally:
        server.running = False
        time.sleep(0.6)
    print("模拟器已退出")


if __name__ == "__main__":
    main()
