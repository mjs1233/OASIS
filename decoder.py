"""
UART 프레임 파서 (COBS 없음, 리틀엔디안)
=========================================

프레임 구조
-----------
HEADER (6 bytes, little-endian)
    magic   : uint16 (0x88E4, '井')
    type    : uint8  (프레임 종류)
    length  : uint16 (뒤에 오는 payload 길이, 그대로의 바이트 수)
    cobs    : uint8  (COBS 미사용. 구조상 자리만 유지, 값은 참고용/미사용)
PAYLOAD (length bytes)
    가공되지 않은 원본 데이터 그대로 (COBS 인코딩 없음)
FOOTER (2 bytes, little-endian)
    magic   : uint16 (0x8BDA, '芹')
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Iterator, List, Optional

# ----------------------------------------------------------------------------
# 상수 정의 (리틀엔디안)
# ----------------------------------------------------------------------------
HEADER_MAGIC = 0x88E4  # '井'
FOOTER_MAGIC = 0x8BDA  # '芹'

HEADER_STRUCT = struct.Struct("<HBHB")  # magic, type, length, cobs
FOOTER_STRUCT = struct.Struct("<H")     # magic
HEADER_SIZE = HEADER_STRUCT.size        # 6
FOOTER_SIZE = FOOTER_STRUCT.size        # 2

MAX_PAYLOAD_LEN = 0xFFFF  # length 필드가 16비트이므로 안전장치용 상한


# ----------------------------------------------------------------------------
# 파싱 결과 프레임
# ----------------------------------------------------------------------------
@dataclass
class Frame:
    type: int
    cobs: int       # COBS 미사용, 헤더에 있던 값 그대로 참고용으로 보관
    payload: bytes  # 가공되지 않은 원본 데이터


class _State(Enum):
    SEARCH_HEADER = auto()
    WAIT_HEADER = auto()
    WAIT_PAYLOAD = auto()
    WAIT_FOOTER = auto()


# ----------------------------------------------------------------------------
# 스트리밍 파서
# ----------------------------------------------------------------------------
class UartFrameParser:
    """UART로부터 조각조각 들어오는 바이트를 받아 완성된 Frame 을 뽑아내는 파서.
    COBS 디코딩은 하지 않고, HEADER.length 만큼의 payload를 그대로 잘라냅니다.

    사용법:
        parser = UartFrameParser(on_error=print)
        while True:
            chunk = ser.read(ser.in_waiting or 1)
            for frame in parser.feed(chunk):
                handle_frame(frame)
    """

    _HEADER_MAGIC_BYTES = HEADER_MAGIC.to_bytes(2, "little")

    def __init__(self, on_text: Optional[Callable[[str], None]] = None):
        """
        on_text: 프레임 형식에 맞지 않는(=불일치) 바이트를 만났을 때 호출됩니다.
                 기본값은 그냥 print() 로 출력합니다.
        """
        self._buf = bytearray()
        self._state = _State.SEARCH_HEADER
        self._type: int = 0
        self._length: int = 0
        self._cobs: int = 0
        self._on_text = on_text or (lambda text: print(text, end=""))

    def feed(self, data: bytes) -> Iterator[Frame]:
        """새로 수신한 바이트를 넣고, 그 결과로 완성된 프레임들을 yield 합니다."""
        self._buf.extend(data)

        while True:
            if self._state == _State.SEARCH_HEADER:
                if not self._sync_to_header():
                    return  # 더 이상 처리할 데이터 없음, 다음 feed() 대기

            if self._state == _State.WAIT_HEADER:
                if len(self._buf) < HEADER_SIZE:
                    return
                header_bytes = bytes(self._buf[:HEADER_SIZE])
                magic, ftype, length, cobs = HEADER_STRUCT.unpack_from(header_bytes, 0)
                del self._buf[:HEADER_SIZE]

                if magic != HEADER_MAGIC:
                    # 이론상 발생하지 않지만 방어적으로 처리: 불일치 -> 텍스트로 출력
                    self._emit_text(header_bytes)
                    self._state = _State.SEARCH_HEADER
                    continue

                if length > MAX_PAYLOAD_LEN:
                    # 비정상 length -> 이 헤더는 진짜 프레임이 아니었던 것으로 보고 텍스트로 출력
                    self._emit_text(header_bytes)
                    self._state = _State.SEARCH_HEADER
                    continue

                self._type = ftype
                self._length = length
                self._cobs = cobs
                self._state = _State.WAIT_PAYLOAD
                continue

            if self._state == _State.WAIT_PAYLOAD:
                if len(self._buf) < self._length:
                    return
                payload = bytes(self._buf[: self._length])
                del self._buf[: self._length]
                self._pending_payload = payload
                self._state = _State.WAIT_FOOTER
                continue

            if self._state == _State.WAIT_FOOTER:
                if len(self._buf) < FOOTER_SIZE:
                    return
                footer_bytes = bytes(self._buf[:FOOTER_SIZE])
                (foot_magic,) = FOOTER_STRUCT.unpack_from(footer_bytes, 0)
                del self._buf[:FOOTER_SIZE]

                if foot_magic != FOOTER_MAGIC:
                    # 불일치 -> 프레임이 아니었던 것으로 보고, 지금까지 읽은 바이트를 그냥 텍스트로 출력
                    header_bytes = HEADER_STRUCT.pack(HEADER_MAGIC, self._type, self._length, self._cobs)
                    self._emit_text(header_bytes + self._pending_payload + footer_bytes, "!!\n")
                    self._state = _State.SEARCH_HEADER
                    continue

                self._state = _State.SEARCH_HEADER
                yield Frame(type=self._type, cobs=self._cobs, payload=self._pending_payload)
                continue

    # ------------------------------------------------------------------
    def _sync_to_header(self) -> bool:
        """버퍼에서 HEADER magic(0x88E4, 리틀엔디안)을 찾아 그 위치까지 앞부분을 버립니다.
        찾으면 True 를 반환하고 상태를 WAIT_HEADER 로 바꿉니다.
        못 찾으면 False 를 반환합니다 (더 받아야 함).
        magic이 아닌(=프레임과 불일치하는) 부분은 버리지 않고 텍스트로 출력합니다.
        """
        idx = self._buf.find(self._HEADER_MAGIC_BYTES)
        if idx == -1:
            # magic이 청크 경계에 걸쳐있을 수 있으므로 마지막 1바이트는 남겨둔다
            if len(self._buf) > 1:
                leftover = bytes(self._buf[:-1])
                del self._buf[:-1]
                self._emit_text(leftover)
            return False

        if idx > 0:
            leftover = bytes(self._buf[:idx])
            del self._buf[:idx]
            self._emit_text(leftover)

        self._state = _State.WAIT_HEADER
        return True

    def _emit_text(self, data: bytes, str = "") -> None:
        """프레임 형식과 맞지 않는 바이트를 텍스트로 출력합니다."""
        if not data:
            return
        self._on_text(str + data.decode("utf-8", errors="replace"))


# ----------------------------------------------------------------------------
# 프레임 인코딩 헬퍼 (송신 측 / 테스트용)
# ----------------------------------------------------------------------------
def build_frame(frame_type: int, payload: bytes, cobs: int = 0) -> bytes:
    """type + 원본 payload 로부터 완전한 프레임(HEADER+payload+FOOTER)을 만듭니다.
    COBS 인코딩은 하지 않으며, payload는 그대로 들어갑니다.
    """
    header = HEADER_STRUCT.pack(HEADER_MAGIC, frame_type, len(payload), cobs)
    footer = FOOTER_STRUCT.pack(FOOTER_MAGIC)
    return header + payload + footer


# ----------------------------------------------------------------------------
# COM 포트 조회
# ----------------------------------------------------------------------------
def list_serial_ports() -> List[str]:
    """현재 시스템에 연결된 COM(시리얼) 포트 이름 목록을 반환합니다."""
    from serial.tools import list_ports  # pip install pyserial

    return [p.device for p in list_ports.comports()]


# ----------------------------------------------------------------------------
# UART 설정
# ----------------------------------------------------------------------------
@dataclass
class UartConfig:
    """UART(시리얼 포트) 통신 설정.

    parity   : 'N'(none), 'E'(even), 'O'(odd), 'M'(mark), 'S'(space)
    stopbits : 1, 1.5, 2
    bytesize : 5, 6, 7, 8
    """

    port: str
    baudrate: int = 115200
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1
    timeout: Optional[float] = 0.1        # 읽기 타임아웃(초). None이면 블로킹
    write_timeout: Optional[float] = 1.0  # 쓰기 타임아웃(초)
    rtscts: bool = False
    xonxoff: bool = False
    dsrdtr: bool = False


def open_uart(config: UartConfig) -> "serial.Serial":
    """UartConfig 설정대로 시리얼 포트를 열어서 반환합니다."""
    import serial  # pip install pyserial

    bytesize_map = {
        5: serial.FIVEBITS,
        6: serial.SIXBITS,
        7: serial.SEVENBITS,
        8: serial.EIGHTBITS,
    }
    parity_map = {
        "N": serial.PARITY_NONE,
        "E": serial.PARITY_EVEN,
        "O": serial.PARITY_ODD,
        "M": serial.PARITY_MARK,
        "S": serial.PARITY_SPACE,
    }
    stopbits_map = {
        1: serial.STOPBITS_ONE,
        1.5: serial.STOPBITS_ONE_POINT_FIVE,
        2: serial.STOPBITS_TWO,
    }

    ser = serial.Serial()
    ser.port = config.port
    ser.baudrate = config.baudrate
    ser.bytesize = bytesize_map[config.bytesize]
    ser.parity = parity_map[config.parity.upper()]
    ser.stopbits = stopbits_map[config.stopbits]
    ser.timeout = config.timeout
    ser.write_timeout = config.write_timeout
    ser.rtscts = config.rtscts
    ser.xonxoff = config.xonxoff
    ser.dsrdtr = config.dsrdtr

    ser.open()
    return ser


# ----------------------------------------------------------------------------
# 실제 시리얼 포트에서 읽는 예시
# ----------------------------------------------------------------------------
def run_serial_example(config: UartConfig):
    parser = UartFrameParser()  # 불일치 데이터는 기본적으로 그냥 print() 로 출력됨

    with open_uart(config) as ser:
        while True:
            chunk = ser.read(ser.in_waiting or 1)
            if not chunk:
                continue
            for frame in parser.feed(chunk):
                print(f"type=0x{frame.type:02X} len={len(frame.payload)} "
                      f"payload={frame.payload.hex()}")


if __name__ == "__main__":
    print("사용 가능한 COM 포트:", list_serial_ports())
    # 사용 예:
    cfg = UartConfig(port="COM3", baudrate=115200)
    run_serial_example(cfg)
