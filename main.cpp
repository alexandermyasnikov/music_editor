
#include <iostream>
#include <cstring>
#include <cassert>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <array>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <cmath>
#include <filesystem>
#include "logger.h"



////////////////////////////////////////////////////////////////////////////////

std::string hex(const void* ptr, size_t len) {
  std::stringstream sstream;
  sstream << "{ " << std::hex;
  for (size_t i{}; i < len; ++i) {
    sstream << std::setfill('0') << std::setw(2) << (int) ((const uint8_t*) ptr)[i] << " ";
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

  size_t size() { return _buf.size() - _pos; }
  bool empty() { return size() == 0; }

  void read_data(void* ptr, size_t len, bool peak = false) {
    if (_pos + len <= _buf.size()) {
      memcpy(ptr, _buf.c_str() + _pos, len);
      if (!peak)
        _pos += len;
      // LOG_MUS("read: '%s' ", hex(ptr, len).c_str());
    } else {
      LOG_MUS("ERROR: '%zd' / '%zd' ", len, size());
      throw std::runtime_error("empty flow");
    }
  }

  void write_data(void* ptr, size_t len) {
    _buf.append((char*) ptr, len);
    // LOG_MUS("write: '%s' ", hex(ptr, len).c_str());
  }

  void skip(size_t len) {
    if (_pos + len <= _buf.size()) {
      // LOG_MUS("skip: '%s' ", hex(_buf.c_str() + _pos, len).c_str());
      _pos += len;
    } else {
      LOG_MUS("ERROR: '%zd' / '%zd' ", len, size());
      throw std::runtime_error("empty flow");
    }
  }

  template <typename T, typename std::enable_if<std::is_pod<T>::value>::type* = nullptr>
  void read(T& value, bool peak = false) {
    read_data(&value, sizeof(T), peak);
  }

  template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
  void read(T& value, bool peak = false) {
    uint32_t size;
    read(size, peak);

    value.resize(size);
    read_data(value.data(), value.size(), peak);
  }

  template <typename T, typename std::enable_if<std::is_pod<T>::value>::type* = nullptr>
  void write(T& value) {
    write_data(&value, sizeof(T));
  }

  template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
  void write(T& value) {
    uint32_t size = value.size();
    write(size);
    write_data(value.data(), value.size());
  }
};

////////////////////////////////////////////////////////////////////////////////

// TODO
// Размер блока в wav должен быть четным.

////////////////////////////////////////////////////////////////////////////////

struct file_wav_t {

  struct riff_hdr_t {
    uint32_t id;
    uint32_t size;
    uint32_t format;
  } __attribute__((packed));

  struct fmt_hdr_t {
    uint32_t id;
    uint32_t size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
  } __attribute__((packed));

  struct data_hdr_t {
    uint32_t id;
    uint32_t size;
  } __attribute__((packed));

  std::vector<std::vector<uint32_t>> frames;
  uint32_t sample_rate;

  void read(const std::string& fname) {
    LOGGER_MUS;

    std::ifstream t(fname);
    std::string buf((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    stream_t stream(buf);

    {
      riff_hdr_t riff_hdr;
      stream.read(riff_hdr);
      LOG_MUS("id:     '0x%x' '%.*s' ", riff_hdr.id, sizeof(riff_hdr.id), &riff_hdr.id);
      LOG_MUS("size:   '0x%x' '%d' ", riff_hdr.size, riff_hdr.size);
      LOG_MUS("format: '0x%x' '%.*s' ", riff_hdr.format, sizeof(riff_hdr.format), &riff_hdr.format);

      if (riff_hdr.id != 0x46464952) { // "RIFF" little-endian
        LOG_MUS("ERROR: invalid id");
        return;
      }

      if (riff_hdr.format != 0x45564157) { // "WAVE" little-endian
        LOG_MUS("ERROR: invalid format");
        return;
      }
    }

    fmt_hdr_t fmt_hdr;
    {
      stream.read(fmt_hdr);
      LOG_MUS("id:              '0x%x' '%.*s' ", fmt_hdr.id, sizeof(fmt_hdr.id), &fmt_hdr.id);
      LOG_MUS("size:            '0x%x' '%d' ", fmt_hdr.size, fmt_hdr.size);
      LOG_MUS("audio_format:    '0x%x' '%d' ", fmt_hdr.audio_format, fmt_hdr.audio_format);
      LOG_MUS("num_channels:    '0x%x' '%d' ", fmt_hdr.num_channels, fmt_hdr.num_channels);
      LOG_MUS("sample_rate:     '0x%x' '%d' ", fmt_hdr.sample_rate, fmt_hdr.sample_rate);
      LOG_MUS("byte_rate:       '0x%x' '%d' ", fmt_hdr.byte_rate, fmt_hdr.byte_rate);
      LOG_MUS("block_align:     '0x%x' '%d' ", fmt_hdr.block_align, fmt_hdr.block_align);
      LOG_MUS("bits_per_sample: '0x%x' '%d' ", fmt_hdr.bits_per_sample, fmt_hdr.bits_per_sample);

      if (fmt_hdr.id != 0x20746d66) { // "fmt " little-endian
        LOG_MUS("ERROR: invalid id");
        return;
      }

      if (fmt_hdr.size != 16) { // 16 for PCM
        LOG_MUS("ERROR: invalid size");
        return;
      }

      if (fmt_hdr.audio_format != 1) { // 1 for PCM
        LOG_MUS("ERROR: invalid size");
        return;
      }

      frames.resize(fmt_hdr.num_channels);
      sample_rate = fmt_hdr.sample_rate;
    }

    {
      data_hdr_t data_hdr;
      stream.read(data_hdr);
      LOG_MUS("id:   '0x%x' '%.*s' ", data_hdr.id, sizeof(data_hdr.id), &data_hdr.id);
      LOG_MUS("size: '0x%x' '%d' ", data_hdr.size, data_hdr.size);

      if (data_hdr.id != 0x61746164) { // "data " little-endian
        LOG_MUS("ERROR: invalid id");
        return;
      }

      if (data_hdr.size > stream.size()) {
        LOG_MUS("stream.size:   '0x%x' '%d' ", stream.size(), stream.size());
        LOG_MUS("data_hdr.size: '0x%x' '%d' ", data_hdr.size, data_hdr.size);
        LOG_MUS("ERROR: broken data");
        data_hdr.size = stream.size();
      }

      size_t frames_per_channel = data_hdr.size / fmt_hdr.block_align;
      LOG_MUS("frames_per_channel %zd", frames_per_channel);

      for (size_t i = 0; i < frames_per_channel; ++i) {
        for (size_t j = 0; j < frames.size(); ++j) {
          uint8_t tmp = 0;
          uint32_t frame = 0;
          for (size_t k = 0; k < fmt_hdr.bits_per_sample / 8; ++k) {
            stream.read(tmp);
            frame |= tmp << 8 * k;
          }
          for (size_t k = fmt_hdr.bits_per_sample / 8; k < 32 / 8; ++k) {
            frame |= tmp << 8 * k;
          }
          frames[j].push_back(frame);
        }
      }

    }

    if (!stream.empty()) {
      LOG_MUS("ERROR: stream is not empty '%zd' ", stream.size());
      stream.skip(stream.size());
    }
  }

  void write(const std::string& fname) {
    LOGGER_MUS;

    stream_t stream;

    // prewrite

    uint8_t bits_per_sample = 32;

    data_hdr_t data_hdr = {
      .id = 0x61746164,
      .size = uint32_t(frames.size() * bits_per_sample / 8 * frames[0].size()),
    };

    fmt_hdr_t fmt_hdr = {
      .id = 0x20746d66,
      .size = 16,
      .audio_format = 1,
      .num_channels = (uint16_t) frames.size(),
      .sample_rate = sample_rate,
      .byte_rate = fmt_hdr.sample_rate * fmt_hdr.num_channels * bits_per_sample / 8,
      .block_align = uint16_t(fmt_hdr.num_channels * bits_per_sample / 8),
      .bits_per_sample = bits_per_sample,
    };

    riff_hdr_t riff_hdr = {
      .id = 0x46464952,
      .size = uint32_t(2 * sizeof(uint32_t) - 4 // XXX
          + 2 * sizeof(uint32_t) + fmt_hdr.size
          + 2 * sizeof(uint32_t) + data_hdr.size),
      .format = 0x45564157,
    };

    // write

    stream.write(riff_hdr);
    stream.write(fmt_hdr);
    stream.write(data_hdr);

    {
      size_t frames_size = frames[0].size(); // XXX
      for (size_t i = 0; i < frames_size; ++i) {
        for (size_t j = 0; j < frames.size(); ++j) {
          uint8_t tmp = 0;
          uint32_t frame = frames[j][i];
          for (size_t k = 0; k < fmt_hdr.bits_per_sample / 8; ++k) {
            tmp = (frame >> 8 * k) & 0xFF;
            stream.write(tmp);
          }
        }
      }
    }

    std::ofstream t(fname);
    t.write(stream._buf.data(), stream._buf.size());
  }
};

////////////////////////////////////////////////////////////////////////////////

struct pattern_t {
  virtual double f(double) = 0;
};

struct pattern_const_t : pattern_t {
  double _level;
  pattern_const_t(double level = 1.0) : _level(level) { }
  double f(double) override { return _level; }
};

struct pattern_sin_t : pattern_t {
  double _k;
  pattern_sin_t(double k = 1) : _k(k) { }
  double f(double x) override { return std::sin(_k * std::acos(-1) * x); }
};

using pattern_sptr_t = std::shared_ptr<pattern_t>;

////////////////////////////////////////////////////////////////////////////////

struct wav_editor_t {
  struct pattern_state_t {
    uint32_t x1;
    uint32_t x2;
    uint32_t x;
    uint32_t y;
    size_t ch_ind;
    pattern_sptr_t pattern;
  };

  std::vector<std::vector<uint32_t>> _channels;
  std::vector<uint32_t>              _result;
  std::vector<pattern_sptr_t>        _patterns;
  std::vector<pattern_state_t>       _pattern_states;
  uint32_t                           _sample_rate = 44100;

  void add_pattern(pattern_sptr_t pattern) { _patterns.push_back(pattern); }

  void load(const std::string& fname) {
    LOGGER_MUS;
    file_wav_t wav;
    wav.read(fname);
    if (_sample_rate == wav.sample_rate) {
      _channels.push_back(wav.frames[0]); // Нужен единственный канал.
    } else {
      LOG_MUS("ERROR: invalid sample_rate");
    }
  }

  void save(const std::string& fname) {
    LOGGER_MUS;
    file_wav_t wav;
    wav.sample_rate = _sample_rate;
    wav.frames.push_back(_result);
    wav.write(fname);
  }

  void process(size_t seconds) {
    LOGGER_MUS;
    _pattern_states.resize(5);
    _result.resize(_sample_rate * seconds);
    for (size_t i = 0; i < _result.size(); ++i) {
      uint32_t frame = 0;

      for (auto& state : _pattern_states) {
        if (!state.pattern || state.x >= state.x2) {
          update_state(state);
          state.y = 0;
          continue;
        }
        double x = (state.x - (double) state.x1) / (state.x2 - state.x1);
        state.y = int32_t(_channels[state.ch_ind][state.x]) * state.pattern->f(x);
        state.x++;
      }

      size_t ind = i % _pattern_states.size();
      frame = _pattern_states[ind].y;

      _result[i] = frame;
    }
  }

  void update_state(pattern_state_t& state) {
    LOGGER_MUS;
    state.ch_ind = rand() % _channels.size();
    size_t pattern_ind = rand() % _patterns.size();
    state.pattern = _patterns[pattern_ind];

    state.x1 = rand() % _channels[state.ch_ind].size();
    state.x2 = state.x1 + (rand() % 10) * _sample_rate;
    state.x = state.x1;
    if (state.x2 >= _channels[state.ch_ind].size()) {
      LOG_MUS("ERROR: invalid state");
      state.pattern = nullptr;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

int main(/*int argc, char* argv[]*/) {

  std::srand(unsigned(std::time(0)));

  wav_editor_t editor;
  editor.add_pattern(std::make_shared<pattern_const_t>(0.0));
  editor.add_pattern(std::make_shared<pattern_const_t>(0.5));
  editor.add_pattern(std::make_shared<pattern_const_t>(1.0));
  editor.add_pattern(std::make_shared<pattern_sin_t>());

  {
    std::string path = "/home/alexander/Music/musicLink/LGND.Media.Empires.The.Worship.(SCENE)-DISCOVER/Kit_01_Arise_(165BPM)/_(WAVs)_Loops";
    for (auto& p : std::filesystem::directory_iterator(path))
      editor.load(p.path());
  }
  {
    std::string path = "/home/alexander/Music/musicLink/LBandyMusicNigeria Afrobeat/Loops - Nigeria Afrobeat/Melody Loops - Nigeria Afrobeat/WAV";
    for (auto& p : std::filesystem::directory_iterator(path))
      editor.load(p.path());
  }

  editor.process(10 * 60);
  editor.save("/mnt/code/cpp/music_spec/wav/out.wav");


  return 0;
}

