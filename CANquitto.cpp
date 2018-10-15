/*
  MIT License

  Copyright (c) 2018 Antonio Alexander Brewer (tonton81) - https://github.com/tonton81

  Designed and tested for PJRC Teensy (3.2, 3.5, 3.6).

  Forum link : https://forum.pjrc.com/threads/53776-CANquitto?p=187492#post187492

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/


#include <CANquitto.h>
#include <kinetis_flexcan.h>
#include <kinetis.h>
#include "Arduino.h"
#include "IFCT.h"
#include <atomic>
#include <util/atomic.h>
#include "Stream.h"

Circular_Buffer<uint8_t, MAX_NODE_RECEIVING * 16, 12> CANquitto::cq_isr_buffer;
Circular_Buffer<uint8_t, (uint32_t)pow(2, ceil(log(MAX_NODE_RECEIVING) / log(2))), MAX_PAYLOAD_SIZE> CANquitto::storage;
Circular_Buffer<uint32_t, 8, 3> CANquitto::nodeBus;
_CQ_ptr CANquitto::_handler = nullptr;
uint32_t CANquitto::nodeNetID;
uint32_t CANquitto::nodeID;
volatile int CANquitto::write_ack_valid = 1;
volatile uint32_t CANquitto::write_id_validate = 0;
bool CANquitto::enabled = 0;
volatile int CANquitto::serial_write_count[6] = { 0 };
volatile int CANquitto::serial_write_response = 0;
volatile int CANquitto::digitalread_response = 0;
volatile int CANquitto::analogread_response = 0;
volatile int CANquitto::available_response = 0;
volatile int CANquitto::peek_response = 0;
volatile int CANquitto::read_response = 0;
volatile int CANquitto::readbuf_response_flag = 0;
volatile uint8_t CANquitto::readbuf_response[6] = { 0 };

CANquitto::CANquitto(uint8_t nodeToControl) {
  Serial.featuredNode = nodeToControl;
  Serial.serial_access = 0x80 | 0UL;
  Serial.port = 0;
  Serial1.featuredNode = nodeToControl;
  Serial1.serial_access = 0x80 | 1UL;
  Serial1.port = 1;
  Serial2.featuredNode = nodeToControl;
  Serial2.serial_access = 0x80 | 2UL;
  Serial2.port = 2;
  Serial3.featuredNode = nodeToControl;
  Serial3.serial_access = 0x80 | 3UL;
  Serial3.port = 3;
  Serial4.featuredNode = nodeToControl;
  Serial4.serial_access = 0x80 | 4UL;
  Serial4.port = 4;
  Serial5.featuredNode = nodeToControl;
  Serial5.serial_access = 0x80 | 5UL;
  Serial5.port = 5;
  Serial6.featuredNode = nodeToControl;
  Serial6.serial_access = 0x80 | 6UL;
  Serial6.port = 6;

  Wire.featuredNode = nodeToControl;
  Wire.wire_access = 0x80 | 0UL;
  Wire.port = 0;
  Wire1.featuredNode = nodeToControl;
  Wire1.wire_access = 0x80 | 1UL;
  Wire1.port = 1;
  Wire2.featuredNode = nodeToControl;
  Wire2.wire_access = 0x80 | 2UL;
  Wire2.port = 2;
  Wire3.featuredNode = nodeToControl;
  Wire3.wire_access = 0x80 | 3UL;
  Wire3.port = 3;

  SPI.featuredNode = nodeToControl;
  SPI.spi_access = 0x80 | 0UL;
  SPI.port = 0;
  SPI1.featuredNode = nodeToControl;
  SPI1.spi_access = 0x80 | 1UL;
  SPI1.port = 1;
  SPI2.featuredNode = nodeToControl;
  SPI2.spi_access = 0x80 | 2UL;
  SPI2.port = 2;
}

bool CANquitto::begin(uint8_t node, uint32_t net) {
  if ( node && node < 128 && net & 0x1FFE0000 ) {
    nodeID = node;
    nodeNetID = (net & 0x1FFE0000);
    return (enabled = 1);
  }
  return (enabled = 0);
}

int CANquitto::digitalRead(uint8_t pin) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return -1;
  CANquitto::digitalread_response = -1;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 1; // DIGITAL READ
  msg.buf[3] = pin; // PIN
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
  uint32_t timeout = millis();
  while ( CANquitto::digitalread_response == -1 ) {
    if ( millis() - timeout > 200 ) return -1;
  }
  return CANquitto::digitalread_response;
}

int CANquitto::analogRead(uint8_t pin) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return -1;
  CANquitto::analogread_response = -1;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 4; // ANALOGREAD
  msg.buf[3] = pin; // PIN
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
  uint32_t timeout = millis();
  while ( CANquitto::analogread_response == -1 ) {
    if ( millis() - timeout > 200 ) return -1;
  }
  return CANquitto::analogread_response;
}

int CANquitto::NodeFeatures::available() {
  if ( !CANquitto::isOnline(featuredNode) ) return -1;
  CANquitto::available_response = -1;
  CAN_message_t msg;
  msg.ext = msg.seq = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 0; // AVAILABLE
    msg.buf[3] = port; // SERIAL_PORT
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
    uint32_t timeout = millis();
    while ( CANquitto::available_response == -1 ) {
      if ( millis() - timeout > 200 ) return -1;
    }
    return CANquitto::available_response;
  }
  return -1;
}

int CANquitto::NodeFeatures::peek() {
  if ( !CANquitto::isOnline(featuredNode) ) return -1;
  CANquitto::peek_response = -1;
  CAN_message_t msg;
  msg.ext = msg.seq = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 1; // PEEK
    msg.buf[3] = port; // SERIAL_PORT
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
    uint32_t timeout = millis();
    while ( CANquitto::peek_response == -1 ) {
      if ( millis() - timeout > 200 ) return -1;
    }
    return CANquitto::peek_response;
  }
  return -1;
}

size_t CANquitto::NodeFeatures::read(uint8_t* buf, size_t size) {
  if ( !CANquitto::isOnline(featuredNode) ) return -1;
  CANquitto::readbuf_response_flag = -1;
  CAN_message_t msg;
  msg.ext = msg.seq = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 3; // read(buffer,length)
    msg.buf[3] = port; // SERIAL_PORT
    IFCT& bus = CANquitto::node_bus(featuredNode);
    uint16_t count = 0;
    for ( uint16_t i = 0; i < size; ) {
      CANquitto::readbuf_response_flag = -1;
      if ( (size-1-i) < 5 ) {
        msg.buf[4] = size-i;
        i = size;
      }
      else {
        msg.buf[4] = 5;
      }
      bus.write(msg);
      uint32_t timeout = millis();
      while ( CANquitto::readbuf_response_flag == -1 ) {
        if ( millis() - timeout > 200 ) break; // EXIT ON ERROR!!!!
      }
      for ( uint16_t k = 0; k < CANquitto::readbuf_response[0]; k++ ) {
        buf[count+k] = CANquitto::readbuf_response[1+k];
      }
      count += readbuf_response[0];
      i+=5;
    }
    return count;
  }
  return 0;
}

int CANquitto::NodeFeatures::read() {
  if ( !CANquitto::isOnline(featuredNode) ) return -1;
  CANquitto::read_response = -1;
  CAN_message_t msg;
  msg.ext = msg.seq = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 2; // READ
    msg.buf[3] = port; // SERIAL_PORT
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
    uint32_t timeout = millis();
    while ( CANquitto::read_response == -1 ) {
      if ( millis() - timeout > 200 ) return -1;
    }
    return CANquitto::read_response;
  }
  return -1;
}

void CANquitto::NodeFeatures::begin(uint32_t baud) {
  if ( !CANquitto::isOnline(featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 4; // BEGIN
    msg.buf[3] = port; // PORT
    msg.buf[4] = ((uint8_t)(baud >> 24)); // BAUDRATE
    msg.buf[5] = ((uint8_t)(baud >> 16)); // BAUDRATE
    msg.buf[6] = ((uint8_t)(baud >> 8)); // BAUDRATE
    msg.buf[7] = ((uint8_t)(baud)); // BAUDRATE
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
  }
}

void CANquitto::NodeFeatures::setRX(uint8_t pin) {
  if ( !CANquitto::isOnline(featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 5; // setRX
    msg.buf[3] = port; // PORT
    msg.buf[4] = pin; // PIN
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
  }
}

void CANquitto::NodeFeatures::setTX(uint8_t pin, bool opendrain) {
  if ( !CANquitto::isOnline(featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 2; // SERIAL PINS
    msg.buf[1] = 0; // SPACEHOLDER
    msg.buf[2] = 6; // setRX
    msg.buf[3] = port; // PORT
    msg.buf[4] = pin; // PIN
    msg.buf[5] = opendrain; // OPTION
    IFCT& bus = CANquitto::node_bus(featuredNode);
    bus.write(msg);
  }
}

void CANquitto::analogReadResolution(unsigned int bits) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 5; // ANALOGREADRESOLUTION
  msg.buf[3] = bits; // BITS
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
}


void CANquitto::digitalWrite(uint8_t pin, uint8_t state) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 0; // DIGITAL WRITE
  msg.buf[3] = pin; // PIN
  msg.buf[4] = state; // STATE
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
}

void CANquitto::pinMode(uint8_t pin, uint8_t mode) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 3; // PINMODE
  msg.buf[3] = pin; // PIN
  msg.buf[4] = mode; // MODE
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
}


void CANquitto::toggle(uint8_t pin) {
  if ( !CANquitto::isOnline(Serial.featuredNode) ) return;
  CAN_message_t msg;
  msg.ext = 1;
  msg.id = CANquitto::nodeNetID | Serial.featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  msg.buf[0] = 1; // HARDWARE PINS
  msg.buf[1] = 0; // GPIO
  msg.buf[2] = 2; // TOGGLE PIN
  msg.buf[3] = pin; // PIN
  IFCT& bus = CANquitto::node_bus(Serial.featuredNode);
  bus.write(msg);
}


uint8_t CANquitto::sendMsg(const uint8_t *array, uint32_t length, uint8_t packetid, uint32_t delay_send, uint32_t wait_time) {
  if ( !length ) return 0;
  if ( !Serial.featuredNode || Serial.featuredNode > 127 ) return 0; // nodes are from 1 to 127 max

  CANquitto::write_ack_valid = 0;
  CANquitto::write_id_validate = Serial.featuredNode;

  if ( !CANquitto::isOnline(Serial.featuredNode) ) return 0;
  IFCT& bus = node_bus(Serial.featuredNode);

  length += 5;
  uint8_t buffer[length];
  memmove(&buffer[5], &array[0], length-5);
  buffer[0] = (length-5) >> 8;
  buffer[1] = (length-5);

  uint16_t checksum = 0;
  for ( uint32_t i = 5; i < length; i++ ) checksum ^= buffer[i];
  buffer[2] = checksum >> 8;
  buffer[3] = checksum;
  buffer[4] = packetid;

  uint16_t buf_levels = (int)ceil((float)length / 6);
  uint8_t buf[buf_levels][8];

  for ( uint16_t i = 0; i < buf_levels; i++ ) {
    if ( i < buf_levels - 1 ) {
      memmove(&buf[i][2], &buffer[i * 6], 6);
      buf[i][0] = (6UL << 4) | (i >> 8);
      buf[i][1] = i;
      continue;
    }
    for ( uint8_t k = (length - (i * 6)) + 2; k < 8; k++ ) buf[i][k] = 0xAA;
    memmove(&buf[i][2], &buffer[i * 6], (length - (i * 6)));
    buf[i][0] = ((length - (i * 6)) << 4) | (i >> 8);
    buf[i][1] = i;
  }

  static CAN_message_t _send;
  _send.ext = _send.seq = 1;

  for ( uint16_t j = 0; j < buf_levels; j++ ) {
    if ( !j ) _send.id = nodeNetID | Serial.featuredNode << 7 | nodeID | 1UL << 14;
    else if ( j == ( buf_levels - 1 ) ) _send.id = nodeNetID | Serial.featuredNode << 7 | nodeID | 3UL << 14;
    else if ( j ) _send.id = nodeNetID | Serial.featuredNode << 7 | nodeID | 2UL << 14;
    memmove(&_send.buf[0], &buf[j][0], 8);
    bus.write(_send);
    delayMicroseconds(delay_send);
  }

  uint32_t timeout = millis();
  while ( CANquitto::write_ack_valid != 0x06 ) {
    if ( millis() - timeout > wait_time ) break; 
    if ( write_ack_valid == 0x15 ) break;
  }
  return (( write_ack_valid ) ? write_ack_valid : 0xFF);
}


bool CANquitto::isOnline(uint8_t node) {
  uint32_t node_bus[3] = { node };
  return CANquitto::nodeBus.find(node_bus, 3, 0, 0, 0);
}


IFCT& CANquitto::node_bus(uint8_t node) {
  uint32_t node_bus[3] = { node, 0 };
  bool found = CANquitto::nodeBus.find(node_bus, 3, 0, 0, 0);
  if ( found ) {
    if ( !node_bus[1] ) return Can0;
#if defined(__MK66FX1M0__)
    else return Can1;
#endif
  }
  return Can0;
}










void ext_output(const CAN_message_t &msg) {
  if ( ( msg.id & 0x1FFE0000 ) != ( CANquitto::nodeNetID & 0x1FFE0000 ) ) return; /* reject unknown net frames */

  if ( ((msg.id & 0x3F80) >> 7) == 0 ) { /* Node global messages */
    uint32_t node[3] = { (msg.id & 0x7F), msg.bus, millis() };
    if (!(CANquitto::nodeBus.replace(node, 3, 0, 0, 0)) ) CANquitto::nodeBus.push_back(node, 3);
    return;
  }

  if ( ((msg.id & 0x3F80) >> 7) != CANquitto::nodeID ) return; /* ignore frames meant for other nodes from here on. */

  if ( ((msg.id & 0x1C000) >> 14) < 4 ) { /* callback payload for this node! */
    uint8_t isr_buffer[12] = { (uint8_t)(msg.id >> 24), (uint8_t)(msg.id >> 16), (uint8_t)(msg.id >> 8), (uint8_t)msg.id };
    memmove(&isr_buffer[0] + 4, &msg.buf[0], 8);
    CANquitto::cq_isr_buffer.push_back(isr_buffer, 12);
    return;
  }

  if ( ((msg.id & 0x1C000) >> 14) == 4 ) { /* RESPONSES TO GIVE TO OTHER NODES */
    switch ( msg.buf[0] ) {
      case 0: {
          if ( msg.buf[1] == 0 ) CANquitto::write_ack_valid = msg.buf[2];
          if ( msg.buf[1] == 1 ) CANquitto::serial_write_response = msg.buf[2]; // response code for serial writing
          if ( msg.buf[1] == 2 && msg.buf[2] == 0 ) CANquitto::digitalread_response = msg.buf[3];
          if ( msg.buf[1] == 2 && msg.buf[2] == 1 ) CANquitto::analogread_response = ((int)(msg.buf[3] << 8) | msg.buf[4]);
          if ( msg.buf[1] == 2 && msg.buf[2] == 2 ) CANquitto::available_response = ((int)(msg.buf[3] << 8) | msg.buf[4]);
          if ( msg.buf[1] == 2 && msg.buf[2] == 3 ) CANquitto::peek_response = ((int)(msg.buf[3] << 8) | msg.buf[4]);
          if ( msg.buf[1] == 2 && msg.buf[2] == 4 ) CANquitto::read_response = ((int)(msg.buf[3] << 8) | msg.buf[4]);
          if ( msg.buf[1] == 3 ) {
            CANquitto::readbuf_response_flag = 1;
            CANquitto::readbuf_response[0] = msg.buf[2];
            for ( uint8_t i = 1; i < 6; i++ ) CANquitto::readbuf_response[i] = msg.buf[i+2];
          }
          break;
        }
    }
    return;
  }

  if ( ((msg.id & 0x1C000) >> 14) == 5 ) { /* COMMANDS GIVEN FROM OTHER NODES TO OPERATE HERE */
    switch ( msg.buf[0] ) {
      case 0: { /* SERIAL WRITE */
          if ( (msg.buf[1] & 0x7) == 0 ) ::Serial.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
          else if ( (msg.buf[1] & 0x7) == 1 ) ::Serial1.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
          else if ( (msg.buf[1] & 0x7) == 2 ) ::Serial2.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
          else if ( (msg.buf[1] & 0x7) == 3 ) ::Serial3.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
          else if ( (msg.buf[1] & 0x7) == 4 ) ::Serial4.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
          else if ( (msg.buf[1] & 0x7) == 5 ) ::Serial5.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
          else if ( (msg.buf[1] & 0x7) == 6 ) ::Serial6.write(msg.buf + 2, ((msg.buf[1] & 0x38) >> 3));
#endif
          if ( ((msg.buf[1] & 0xC0) >> 6) == 0 ) CANquitto::serial_write_count[(msg.buf[1] & 0x7)] = ((msg.buf[1] & 0x38) >> 3);
          else if ( ((msg.buf[1] & 0xC0) >> 6) == 1 ) CANquitto::serial_write_count[(msg.buf[1] & 0x7)] += ((msg.buf[1] & 0x38) >> 3);
          else if ( ((msg.buf[1] & 0xC0) >> 6) == 2 ) {
            CANquitto::serial_write_count[(msg.buf[1] & 0x7)] += ((msg.buf[1] & 0x38) >> 3);
            CAN_message_t response;
            response.buf[1] = response.ext = 1;
            response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
            response.buf[2] = CANquitto::serial_write_count[(msg.buf[1] & 0x7)];
            CANquitto::serial_write_count[(msg.buf[1] & 0x7)] = 0;
            if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
            else Can1.write(response);
#endif
          }
          break;
        }
      case 1: { /* HARDWARE PINS SECTION */
          switch ( msg.buf[1] ) { /* GPIO AREA */
            case 0: {
                switch ( msg.buf[2] ) {
                  case 0: {
                      digitalWriteFast(msg.buf[3], msg.buf[4]);
                      break;
                    }
                  case 1: {
                      CAN_message_t response;
                      response.buf[1] = 2; // HARDWARE PINS RESPONSE
                      response.buf[2] = 0; // DIGITALREAD
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      response.buf[3] = digitalReadFast(msg.buf[3]);
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 2: {
                      if ( LED_BUILTIN == msg.buf[3] ) GPIOC_PTOR = 32;
                      else digitalWrite(msg.buf[3], !digitalRead(msg.buf[3]) );
                      break;
                    }
                  case 3: {
                      pinMode(msg.buf[3], msg.buf[4]);
                      break;
                    }
                  case 4: {
                      CAN_message_t response;
                      response.buf[1] = 2; // HARDWARE PINS RESPONSE
                      response.buf[2] = 1; // ANALOGREAD
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      int value = analogRead(msg.buf[3]);
                      response.buf[3] = value >> 8;
                      response.buf[4] = value;
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 5: {
                      analogReadResolution(msg.buf[3]);
                      break;
                    }
                }
                break;
              } // GPIO CASE AREA
          } // GPIO SWITCH AREA
          break;
        } // HARDWARE PINS SECTION
      case 2: { /* SERIAL PINS SECTION */
          switch ( msg.buf[1] ) { /* SPACEHOLDER */
            case 0: {
                switch ( msg.buf[2] ) {
                  case 0: { // SERIAL_AVAILABLE
                      CAN_message_t response;
                      response.buf[1] = 2; // HARDWARE PINS RESPONSE
                      response.buf[2] = 2; // AVAILABLE_RESPONSE
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      int value = 0;
                      if ( msg.buf[3] == 0 ) value = ::Serial.available();
                      else if ( msg.buf[3] == 1 ) value = ::Serial1.available();
                      else if ( msg.buf[3] == 2 ) value = ::Serial2.available();
                      else if ( msg.buf[3] == 3 ) value = ::Serial3.available();
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) value = ::Serial4.available();
                      else if ( msg.buf[3] == 5 ) value = ::Serial5.available();
                      else if ( msg.buf[3] == 6 ) value = ::Serial6.available();
#endif
                      response.buf[3] = value >> 8;
                      response.buf[4] = value;
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 1: { // PEEK_AVAILABLE
                      CAN_message_t response;
                      response.buf[1] = 2; // HARDWARE PINS RESPONSE
                      response.buf[2] = 3; // PEEK_RESPONSE
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      int value = 0;
                      if ( msg.buf[3] == 0 ) value = ::Serial.peek();
                      else if ( msg.buf[3] == 1 ) value = ::Serial1.peek();
                      else if ( msg.buf[3] == 2 ) value = ::Serial2.peek();
                      else if ( msg.buf[3] == 3 ) value = ::Serial3.peek();
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) value = ::Serial4.peek();
                      else if ( msg.buf[3] == 5 ) value = ::Serial5.peek();
                      else if ( msg.buf[3] == 6 ) value = ::Serial6.peek();
#endif
                      response.buf[3] = value >> 8;
                      response.buf[4] = value;
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 2: { // READ_AVAILABLE
                      CAN_message_t response;
                      response.buf[1] = 2; // HARDWARE PINS RESPONSE
                      response.buf[2] = 4; // READ_RESPONSE
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      int value = 0;
                      if ( msg.buf[3] == 0 ) value = ::Serial.read();
                      else if ( msg.buf[3] == 1 ) value = ::Serial1.read();
                      else if ( msg.buf[3] == 2 ) value = ::Serial2.read();
                      else if ( msg.buf[3] == 3 ) value = ::Serial3.read();
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) value = ::Serial4.read();
                      else if ( msg.buf[3] == 5 ) value = ::Serial5.read();
                      else if ( msg.buf[3] == 6 ) value = ::Serial6.read();
#endif
                      response.buf[3] = value >> 8;
                      response.buf[4] = value;
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 3: {
                      CAN_message_t response;
                      response.buf[1] = 3; // SERIAL BUFFER READING
                      Stream* port = &::Serial;
                      if ( msg.buf[3] == 0 ) port = &::Serial;
                      else if ( msg.buf[3] == 1 ) port = &::Serial1;
                      else if ( msg.buf[3] == 2 ) port = &::Serial2;
                      else if ( msg.buf[3] == 3 ) port = &::Serial3;
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) port = &::Serial4;
                      else if ( msg.buf[3] == 5 ) port = &::Serial5;
                      else if ( msg.buf[3] == 6 ) port = &::Serial6;
#endif
                      uint16_t count = port->available();
                      ( count > 5 ) ? response.buf[2] = 5 : response.buf[2] = count;
                      if ( port->available() >= msg.buf[4] ) for ( uint8_t i = 0; i < msg.buf[4]; i++ ) response.buf[i+3] = port->read();
                      else for ( uint8_t i = 0; i < count; i++ ) response.buf[i+3] = port->read();
                      response.ext = 1;
                      response.id = (CANquitto::nodeNetID | (msg.id & 0x7F) << 7 | CANquitto::nodeID | (4UL << 14));
                      if ( !msg.bus ) Can0.write(response);
#if defined(__MK66FX1M0__)
                      else Can1.write(response);
#endif
                      break;
                    }
                  case 4: {
                      if ( msg.buf[3] == 0 ) ::Serial.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
                      else if ( msg.buf[3] == 1 ) ::Serial1.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
                      else if ( msg.buf[3] == 2 ) ::Serial2.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
                      else if ( msg.buf[3] == 3 ) ::Serial3.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) ::Serial4.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
                      else if ( msg.buf[3] == 5 ) ::Serial5.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
                      else if ( msg.buf[3] == 6 ) ::Serial6.begin((uint32_t)(msg.buf[4] << 24) | msg.buf[5] << 16 | msg.buf[6] << 8 | msg.buf[7]);
#endif
                      break;
                    }
                  case 5: {
                      if ( msg.buf[3] == 1 ) ::Serial1.setRX(msg.buf[4]);
                      else if ( msg.buf[3] == 2 ) ::Serial2.setRX(msg.buf[4]);
                      else if ( msg.buf[3] == 3 ) ::Serial3.setRX(msg.buf[4]);
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) ::Serial4.setRX(msg.buf[4]);
                      else if ( msg.buf[3] == 5 ) ::Serial5.setRX(msg.buf[4]);
                      else if ( msg.buf[3] == 6 ) ::Serial6.setRX(msg.buf[4]);
#endif
                      break;
                    }
                  case 6: {
                      if ( msg.buf[3] == 1 ) ::Serial1.setTX(msg.buf[4], msg.buf[5]);
                      else if ( msg.buf[3] == 2 ) ::Serial2.setTX(msg.buf[4], msg.buf[5]);
                      else if ( msg.buf[3] == 3 ) ::Serial3.setTX(msg.buf[4], msg.buf[5]);
#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
                      else if ( msg.buf[3] == 4 ) ::Serial4.setTX(msg.buf[4], msg.buf[5]);
                      else if ( msg.buf[3] == 5 ) ::Serial5.setTX(msg.buf[4], msg.buf[5]);
                      else if ( msg.buf[3] == 6 ) ::Serial6.setTX(msg.buf[4], msg.buf[5]);
#endif
                      break;
                    }
                  case 7: {
                      break;
                    }
                  case 8: {
                      break;
                    }
                  case 9: {
                      break;
                    }
                }
                break;
              }
          }
          break;
        }
      case 3: { /* UNUSED */
          break;
        }
      case 4: { /* UNUSED */
          break;
        }
    }
    return;
  }

}



uint16_t ext_events() {
  static uint32_t notify_self = millis();
  if ( millis() - notify_self >= NODE_KEEPALIVE ) { // broadcast self to network
    notify_self = millis();
    static CAN_message_t notifier;
    notifier.ext = 1;
    notifier.id = ( CANquitto::nodeNetID | CANquitto::nodeID );
    Can0.write(notifier);
#if defined(__MK66FX1M0__)
    Can1.write(notifier);
#endif
  }

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    for ( uint16_t i = 0; i < CANquitto::nodeBus.size(); i++ ) {
      static uint32_t node_scan[3]; // remove expired nodes from active list
      CANquitto::nodeBus.peek_front(node_scan, 3, i);
      if ( millis() - node_scan[2] > NODE_UPTIME_LIMIT ) CANquitto::nodeBus.findRemove(node_scan, 3, 0, 1, 2);
    }
  }

  if ( CANquitto::cq_isr_buffer.size() ) {
    uint8_t cq_buffer[12] = { 0 };
    CANquitto::cq_isr_buffer.pop_front(cq_buffer, 12);
    uint32_t cq_canId = (uint32_t)(cq_buffer[0] << 24) | cq_buffer[1] << 16 | cq_buffer[2] << 8 | cq_buffer[3];
    uint8_t buffer[MAX_PAYLOAD_SIZE] = { (uint8_t)(cq_canId & 0x7F) };
    if ( CANquitto::storage.find(buffer, MAX_PAYLOAD_SIZE, 0, 0, 0) ) {
      uint16_t frame_sequence = ((uint16_t)(cq_buffer[4] << 8) | cq_buffer[5]) & 0xFFF;
      if ( frame_sequence ) {
        memmove(&buffer[0] + 7 + ((frame_sequence - 1) * 6), &cq_buffer[0] + 6, 6);
        if ( ((cq_canId & 0x1C000) >> 14) == 3 ) {
          CANquitto::storage.findRemove(buffer, MAX_PAYLOAD_SIZE, 0, 0, 0);
          AsyncCQ info;
          info.node = buffer[0];
          info.packetid = buffer[5];
          uint16_t crc = 0;
          for ( uint16_t i = 0; i < ((uint16_t)(buffer[1] << 8) | buffer[2]); i++ ) crc ^= buffer[i + 6];
          IFCT& bus = CANquitto::node_bus(buffer[0]);
          CAN_message_t response;
          response.ext = 1;
          response.id = (CANquitto::nodeNetID | (uint32_t)buffer[0] << 7 | CANquitto::nodeID | (4UL << 14));
          response.buf[2] = 0x15;
          if ( crc == ((uint16_t)(buffer[3] << 8) | buffer[4]) ) {
            response.buf[2] = 0x06;
            bus.write(response);
            if ( CANquitto::_handler ) CANquitto::_handler(buffer + 6, ((uint16_t)(buffer[1] << 8) | buffer[2]), info);
          }
          else bus.write(response);
        }
        else CANquitto::storage.replace(buffer, MAX_PAYLOAD_SIZE, 0, 0, 0);
      }
    }
    else {
      memmove(&buffer[0] + 1, &cq_buffer[0] + 6, 6);
      CANquitto::storage.push_back(buffer, MAX_PAYLOAD_SIZE);
    }
  }
  return 0;
}


















































size_t CANquitto::NodeFeatures::println(const char *p) {
  char _text[strlen(p) + 1];
  memmove(&_text[0],&p[0],strlen(p));
  _text[sizeof(_text) - 1] = '\n';
  return write((const uint8_t*)_text, sizeof(_text));
}
size_t CANquitto::NodeFeatures::write(const uint8_t *buf, size_t size) {
  if ( !CANquitto::isOnline(featuredNode) ) return 0;
  CAN_message_t msg;
  CANquitto::serial_write_response = -1;
  msg.ext = msg.seq = 1;
  msg.id = CANquitto::nodeNetID | featuredNode << 7 | CANquitto::nodeID | 5UL << 14;
  if ( serial_access ) {
    msg.buf[0] = 0; // UART TX
    msg.buf[1] = 0x30 | port; // WRITE BIT SF (7-8), 6 DATA(3-5) DEFAULT
    bool firstFrameComplete = 1;
    IFCT& bus = CANquitto::node_bus(featuredNode);
    for ( uint16_t i = 0; i < size; i++ ) {
      if ( (size - i) <= 6 ) {
        msg.buf[1] = 0x80 | port | ((size - i) << 3);
        for ( uint8_t k = 2; k < 8; k++ ) {
          if ( i == size ) {
            msg.buf[k] = 0xAA;
            continue;
          }
          msg.buf[k] = buf[i++];
        }
        bus.write(msg);
        break;
      }
      for ( uint8_t k = 2; k < 8; k++ ) msg.buf[k] = buf[i++];
      i--;
      bus.write(msg);
      if ( firstFrameComplete ) {
        firstFrameComplete = 0;
        msg.buf[1] = 0x70 | port;
      }
    }
  }
  uint32_t timeout = millis();
  while ( CANquitto::serial_write_response == -1 ) {
    if ( millis() - timeout > 100 ) return 0;
  }
  return CANquitto::serial_write_response;
}
