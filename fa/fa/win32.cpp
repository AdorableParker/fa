#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <string>
#include <string_view>



//std::string _conv_to_multi (std::wstring_view _old, UINT ToType) {
//	int lenOld = lstrlenW (_old.data ());
//	int lenNew = ::WideCharToMultiByte (ToType, 0, _old.data (), lenOld, nullptr, 0, nullptr, nullptr);
//	std::string s;
//	s.resize (lenNew);
//	if (!::WideCharToMultiByte (ToType, 0, _old.data (), lenOld, const_cast<char *>(s.c_str ()), lenNew, nullptr, nullptr))
//		return "";
//	return s.c_str ();
//}
//std::string utf16_to_gb18030 (std::wstring_view _old) { return _conv_to_multi (_old, CP_ACP); }

LPWCH get_env (std::string _key, std::string _val) {
	::SetEnvironmentVariableA (_key.data (), _val.data ());
	return ::GetEnvironmentStringsW ();
}

// �������̣����ȴ�����ִ�н���������ȡ����
std::string get_process_output (std::string _cmd, LPWCH _env) {
	//FILE *_p = _popen (_cmd.c_str (), "r");
	//if (!_p)
	//	return "";
	//faw::String _ret = "";
	//char buf [MAX_PATH] = { 0 };
	//while (fgets (buf, MAX_PATH, _p)) {
	//	_ret += buf;
	//}
	//_pclose (_p);
	//_ret.replace_self ("\n", "\r\n");
	//return _ret.stra ();
	HANDLE hRead = NULL, hWrite = NULL;
	SECURITY_ATTRIBUTES sa { sizeof (SECURITY_ATTRIBUTES), nullptr, TRUE };
	if (!::CreatePipe (&hRead, &hWrite, &sa, 0))
		return "";
	STARTUPINFOA si = { sizeof (STARTUPINFOA) };
	::GetStartupInfoA (&si);
	//si.hStdInput = hRead;
	si.hStdOutput = hWrite;
	si.hStdError = hWrite;
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = { 0 };
	BOOL bRet = ::CreateProcessA (nullptr, &_cmd [0], nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, _env, nullptr, &si, &pi);
	if (!bRet) {
		::CloseHandle (hRead);
		::CloseHandle (hWrite);
		return "";
	}
	::CloseHandle (pi.hThread);
	DWORD _exit_code = STILL_ACTIVE, _read_num = 0, _need_read;
	char buf [1024];
	std::string _ret = "";
	do {
		::GetExitCodeProcess (pi.hProcess, &_exit_code);
		_need_read = _read_num = 0;
		::PeekNamedPipe (hRead, NULL, 0, NULL, &_need_read, NULL);
		if (_need_read == 0) {
			::Sleep (100);
			continue;
		}
		_need_read = (_need_read > 1024 ? 1024 : _need_read);
		if (::ReadFile (hRead, buf, _need_read, &_read_num, NULL)) {
			_ret.append (buf, (size_t) _read_num);
		}
	} while (_exit_code == STILL_ACTIVE);
	::CloseHandle (pi.hProcess);
	::CloseHandle (hRead);
	::CloseHandle (hWrite);
	return _ret;
}
