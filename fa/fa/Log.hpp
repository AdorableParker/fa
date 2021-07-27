#ifndef __LOG_HPP__
#define __LOG_HPP__



#include <iostream>
#include <string>
#include <string_view>
#include <tuple>

#include <fmt/core.h>
#include <antlr4-runtime/Token.h>



class Log {
	static std::string _process_file (std::string _file) {
		size_t _p = _file.rfind ('/');
		size_t _q = _file.rfind ('\\');
		if (_p == std::string::npos) {
			if (_q == std::string::npos) {
				return _file;
			} else {
				return _file.substr (_q + 1);
			}
		} else {
			if (_q == std::string::npos) {
				return _file.substr (_p + 1);
			} else {
				return _file.substr ((_p > _q ? _p : _q) + 1);
			}
		}
	}

	static void _print_line_code (antlr4::Token *_t) {
		size_t _start = _t->getStartIndex () - _t->getCharPositionInLine ();
		size_t _end1 = s_code.find ('\r', _start), _end2 = s_code.find ('\n', _start);
		if (_end2 != std::string::npos) {
			if (_end1 == std::string::npos) {
				_end1 = _end2;
			} else {
				_end1 = std::min (_end1, _end2);
			}
		}
		std::string_view _line = s_code.substr (_start, _end1 - _start);
		std::string _arrow = "";
		for (size_t i = 0, _end2 = _t->getCharPositionInLine (); i < _end2; ++i) {
			_arrow += s_code [_start + i] == '\t' ? '\t' : ' ';
		}
		_arrow += '^';
		std::cout << _line << std::endl << _arrow << std::endl;
	}

public:
	static void SetCurrentFile (std::string _file, std::string_view _code) {
		s_file = _process_file (_file);
		s_code = _code;
	}
	static void Info (const char *_file, int _line, antlr4::Token *_t, std::string _data) {
		std::string _prefix = fmt::format ("[{}:{}] ", _process_file (_file), _line);
		std::string _content = _t ? fmt::format ("λ�� [{}:{} pos {}] ����Ϣ��", s_file, _t->getLine (), _t->getCharPositionInLine ()): "";
		std::cout << fmt::format ("{}{}{}", _prefix, _content, _data) << std::endl;
		_print_line_code (_t);
	}

	static void Warning (const char *_file, int _line, antlr4::Token *_t, std::string _data) {
		std::string _prefix = fmt::format ("[{}:{}] ", _process_file (_file), _line);
		std::string _content = _t ? fmt::format ("λ�� [{}:{} pos {}] �ľ��棺", s_file, _t->getLine (), _t->getCharPositionInLine ()): "";
		std::cout << fmt::format ("{}{}{}", _prefix, _content, _data) << std::endl;
		_print_line_code (_t);
	}

	static void Error (const char *_file, int _line, antlr4::Token *_t, std::string _data) {
		std::string _prefix = fmt::format ("[{}:{}] ", _process_file (_file), _line);
		std::string _content = _t ? fmt::format ("λ�� [{}:{} pos {}] �Ĵ���", s_file, _t->getLine (), _t->getCharPositionInLine ()) : "";
		std::cout << fmt::format ("{}{}{}", _prefix, _content, _data) << std::endl;
		_print_line_code (_t);
	}

	static std::string s_file;
	static std::string_view s_code;
};

__declspec (selectany) std::string Log::s_file = "";
__declspec (selectany) std::string_view Log::s_code = "";

#define LOG_INFO(t,data)		Log::Info(__FILE__,__LINE__,t,data)
#define LOG_WARNING(t,data)		Log::Warning(__FILE__,__LINE__,t,data)
#define LOG_ERROR(t,data)		Log::Error(__FILE__,__LINE__,t,data)
#define LOG_TODO(t)				Log::Error(__FILE__,__LINE__,t,"�˹�����δ���")



#endif //__LOG_HPP__
