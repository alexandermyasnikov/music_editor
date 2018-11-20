
#include <iostream>
#include <cstring>
#include <cassert>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <array>
#include <vector>
#include <algorithm>
#include "logger.h"



////////////////////////////////////////////////////////////////////////////////

std::string hex(const void* ptr, size_t len) {
  std::stringstream sstream;
  sstream << "{ " << std::hex;
  for (size_t i{}; i < len; ++i) {
    sstream << "0x" << (int) ((const uint8_t*) ptr)[i] << " ";
  }
  sstream << "} " << std::dec << len << "s";
  return sstream.str();
}

void ntoh_base(uint8_t* value, size_t len)
{
  char tmp;
  for (size_t i{}; i < len / 2; ++i) {
    tmp = value[i];
    value[i] = value[len - 1 - i];
    value[len - 1 - i] = tmp;
  }
}

template <typename T>
void ntoh(T& value) {
  ntoh_base((uint8_t*) &value, sizeof(value));
}

////////////////////////////////////////////////////////////////////////////////

struct stream_t {
  std::string _buf;
  size_t _pos;

  stream_t(const std::string& buf = std::string()) : _buf(buf), _pos(0) { }

  bool empty() { return _pos == _buf.size(); }

  void read_data(void* ptr, size_t len, bool peak = false) {
    if (_pos + len <= _buf.size()) {
      memcpy(ptr, _buf.c_str() + _pos, len);
      if (!peak)
        _pos += len;
      // LOG_SNI("read: '%s' ", hex(ptr, len).c_str());
    } else {
      throw std::runtime_error("empty flow");
    }
  }

  void write_data(void* ptr, size_t len) {
    _buf.append((char*) ptr, len);
    // LOG_SNI("write: '%s' ", hex(ptr, len).c_str());
  }

  void skip(size_t len) {
    LOGGER_SNI;
    if (_pos + len <= _buf.size()) {
      LOG_SNI("skip: '%s' ", hex(_buf.c_str() + _pos, len).c_str());
      _pos += len;
    } else {
      throw std::runtime_error("empty flow");
    }
  }

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
  void read(T& value, bool peak = false) {
    read_data(&value, sizeof(T), peak);
    ntoh(value);
  }

  template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
  void read(T& value, bool peak = false) {
    uint32_t size;
    read(size, peak);

    value.resize(size);
    read_data(value.data(), value.size(), peak);
  }

  template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
  void write(T& value) {
    ntoh(value);
    write_data(&value, sizeof(T));
    ntoh(value);
  }

  template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
  void write(T& value) {
    uint32_t size = value.size();
    write(size);
    write_data(value.data(), value.size());
  }
};

////////////////////////////////////////////////////////////////////////////////

struct file_aiff_t {
  // NAME
  std::string _name;

  // COMM
  uint16_t _num_channels;
  uint32_t _num_sample_frames;
  uint16_t _sample_size;
  std::array<uint8_t, 10> _sample_rate;

  // SSND
  uint32_t _offset;
  uint32_t _block_size;
  std::vector<std::vector<uint32_t>> _frames;

  void read(const std::string& fname) {
    LOGGER_SNI;

    std::ifstream t(fname);
    std::string buf((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    stream_t stream(buf);

    uint32_t id;
    stream.read(id);
    LOG_SNI("id: '0x%x' '%.*s' ", id, sizeof(id), &id);
    if (id != 0x464f524d) { // "FORM"
      LOG_SNI("ERROR: invalid id");
      return;
    }

    uint32_t size;
    stream.read(size);
    LOG_SNI("size: '0x%x' '%d' ", size, size);

    uint32_t form_type;
    stream.read(form_type);
    LOG_SNI("form_type: '0x%x' '%.*s' ", form_type, sizeof(form_type), &form_type);
    if (form_type != 0x41494646) { // "AIFF"
      LOG_SNI("ERROR: invalid form_type");
      return;
    }

    while (!stream.empty()) {
      uint32_t id;
      stream.read(id);
      LOG_SNI("id: '0x%x' '%.*s' ", id, sizeof(id), &id);

      switch (id) {
        case 0x4e414d45: { // NAME
          stream.read(_name);
          LOG_SNI("name: '%s' ", _name.c_str());
          break;
        }
        case 0x434f4d4d: { // COMM
          uint32_t size;
          stream.read(size);
          LOG_SNI("size: '0x%x' '%d' ", size, size);

          stream.read(_num_channels);
          LOG_SNI("num_channels: '0x%x' '%d' ", _num_channels, _num_channels);

          stream.read(_num_sample_frames);
          LOG_SNI("num_sample_frames: '0x%x' '%d' ", _num_sample_frames, _num_sample_frames);

          stream.read(_sample_size);
          LOG_SNI("sample_size: '0x%x' '%d' ", _sample_size, _sample_size);

          stream.read_data(_sample_rate.data(), _sample_rate.size());
          break;
        }
        case 0x53534e44: { // SSND
          uint32_t size;
          stream.read(size);
          LOG_SNI("size: '0x%x' '%d' ", size, size);

          stream.read(_offset);
          LOG_SNI("offset: '0x%x' '%d' ", _offset, _offset);

          stream.read(_block_size);
          LOG_SNI("block_size: '0x%x' '%d' ", _block_size, _block_size);

          _frames.resize(_num_channels);
          for (size_t j = 0; j < _num_channels; ++j) {
            _frames[j].resize(_num_sample_frames);
          }

          for (size_t i = 0; i < _num_sample_frames; ++i) {
            for (size_t j = 0; j < _num_channels; ++j) {
              if (_sample_size <= 8) {
                uint8_t frame;
                stream.read(frame);
                _frames[j][i] = frame;
              } else if (_sample_size <= 16) {
                uint16_t frame;
                stream.read(frame);
                _frames[j][i] = frame;
              } else if (_sample_size <= 24) {
                stream.skip(3);
                LOG_SNI("ERROR: _sample_size == 24");
              } else if (_sample_size <= 32) {
                uint32_t frame;
                stream.read(frame);
                _frames[j][i] = frame;
              }
            }
          }
          break;
        }
        default: {
          LOG_SNI("ERROR: unknown id");
          return;
        }
      }
    }
  }

  void write(const std::string& fname) {
    LOGGER_SNI;

    stream_t stream;

    // prewrite

    uint32_t name_id = 0x4e414d45;
    uint32_t name_size_hidden = sizeof(uint32_t) + _name.size();

    uint32_t comm_id = 0x434f4d4d;
    uint32_t comm_size = sizeof(_num_channels) + sizeof(_num_sample_frames)
      + sizeof(_sample_size) + sizeof(_sample_rate);

    uint32_t ssnd_id = 0x53534e44;
    uint32_t ssnd_size = sizeof(_offset) + sizeof(_block_size)
      + _num_sample_frames * _num_channels * ((_sample_size + 7) / 8);

    uint32_t hdr_id = 0x464f524d;
    uint32_t hdr_size = sizeof(uint32_t/*form_type*/) + sizeof(uint32_t) + name_size_hidden
      + 2 * sizeof(uint32_t) + comm_size + 2 * sizeof(uint32_t) + ssnd_size;
    uint32_t hdr_form_type = 0x41494646;

    // write

    stream.write(hdr_id);
    stream.write(hdr_size);
    stream.write(hdr_form_type);

    stream.write(name_id);
    stream.write(_name);

    stream.write(comm_id);
    stream.write(comm_size);
    stream.write(_num_channels);
    stream.write(_num_sample_frames);
    stream.write(_sample_size);
    stream.write_data(_sample_rate.data(), _sample_rate.size());

    stream.write(ssnd_id);
    stream.write(ssnd_size);
    stream.write(_offset);
    stream.write(_block_size);

    for (size_t i = 0; i < _num_sample_frames; ++i) {
      for (size_t j = 0; j < _num_channels; ++j) {
        if (_sample_size <= 8) {
          uint8_t frame = _frames[j][i];
          stream.write(frame);
        } else if (_sample_size <= 16) {
          uint16_t frame = _frames[j][i];
          stream.write(frame);
        } else if (_sample_size <= 24) {
          LOG_SNI("ERROR: _sample_size == 24");
        } else if (_sample_size <= 32) {
          uint32_t frame = _frames[j][i];
          stream.write(frame);
        } else {
          LOG_SNI("ERROR: _sample_size >= 32");
        }
      }
    }

    std::ofstream t(fname);
    t.write(stream._buf.data(), stream._buf.size());
  }
};

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {

  if (argc != 3) {
    std::cerr << "usage: <programm> <in.ext> <out.ext> " << std::endl;
    return 1;
  }

  file_aiff_t aiff;
  aiff.read(argv[1]);
  {
    auto& v = aiff._frames[0];
    std::random_shuffle(v.begin(), v.end());
    for (size_t i = 0; i < aiff._frames[0].size(); ++i) {
      uint32_t& a = aiff._frames[0][i];
      uint32_t& b = aiff._frames[1][i];
      b = a;
      // printf("%8x, %8x, %8x \n", i, a, b);
    }
  }
  aiff.write(argv[2]);

  return 0;
}

