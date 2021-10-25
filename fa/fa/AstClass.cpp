#include "AstValue.h"
#include "AstClass.h"
#include "FuncType.h"

#include <format>
#include <set>



AstClassVarFunc::AstClassVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func)
	: m_pv (_pv), m_name (_name), m_func (_func) {}



// ���Ա����
AstClassItemType AstClassVar::GetType () { return m_var_funcs.size () > 0 ? AstClassItemType::GetterSetter : AstClassItemType::Var; }
std::optional<AstValue> AstClassVar::GetAstValue () {
	LOG_TODO (nullptr);
	if (!m_is_static)
		return std::nullopt;
	// TODO �˴���ȫ�־�̬����
	return std::nullopt;
}

bool AstClassVar::IsStatic () { return m_is_static; }

std::string AstClassVar::GetStringType () { return m_type; }

AstClassVar::AstClassVar (antlr4::Token* _t, PublicLevel _pv, bool _is_static, std::string _type, std::string _name)
	: m_t (_t), m_pv (_pv), m_is_static (_is_static), m_type (_type), m_name (_name) {}

void AstClassVar::SetInitValue (FaParser::ExprContext* _init_value) { m_init_value = _init_value; }

std::tuple<bool, std::string> AstClassVar::AddVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func) {
	static std::set<std::string> s_valid_names { "get", "set" };
	if (!s_valid_names.contains (_name))
		return { false, std::format ("�������֧�� {} ����", _name) };

	for (auto& _vfunc : m_var_funcs) {
		if (_vfunc.m_name == _name)
			return { false, std::format ("{} �����Ѵ���", _name) };
	}
	m_var_funcs.push_back (AstClassVarFunc { _pv, _name, _func });
	return { true, "" };
}



// ���Ա����
AstClassItemType AstClassFunc::GetType () { return AstClassItemType::Func; }

std::optional<AstValue> AstClassFunc::GetAstValue (std::shared_ptr<FuncTypes> _global_funcs) {
	auto _of = _global_funcs->GetFunc (m_name_abi);
	if (!_of.has_value ()) {
		LOG_ERROR (nullptr, "������Ӧ���Ų�����");
		return std::nullopt;
	}
	auto _fp = _global_funcs->GetFuncPtr (m_name_abi);
	return AstValue { _of.value (), _fp };
}

bool AstClassFunc::IsStatic () { return m_is_static; }

std::string AstClassFunc::GetStringType () { return m_name_abi; }

AstClassFunc::AstClassFunc (PublicLevel _pv, bool _is_static, std::string _name)
	: m_pv (_pv), m_is_static (_is_static), m_name (_name) {}

void AstClassFunc::SetReturnType (FaParser::TypeContext* _ret_type_raw) {
	m_ret_type = _ret_type_raw->getText ();
	m_ret_type_t = _ret_type_raw->start;
}

void AstClassFunc::SetArgumentTypeName (FaParser::TypeContext* _arg_type_raw, std::string _name) {
	SetArgumentTypeName (_arg_type_raw->getText (), _arg_type_raw->start, _name);
}

void AstClassFunc::SetArgumentTypeName (std::string _arg_type, antlr4::Token* _arg_type_t, std::string _name) {
	m_arg_types.push_back (_arg_type);
	m_arg_type_ts.push_back (_arg_type_t);
	m_arg_names.push_back (_name);
}

void AstClassFunc::SetFuncBody (FaParser::ClassFuncBodyContext* _func) { m_func = _func; }



// ö�����ͳ�Ա
AstClassItemType AstClassEnumItem::GetType () { return AstClassItemType::EnumItem; }

std::optional<AstValue> AstClassEnumItem::GetAstValue () {
	// TODO ���س���ֵ
	return std::nullopt;
}

bool AstClassEnumItem::IsStatic () { return true; }

std::string AstClassEnumItem::GetStringType () { return "TODO???????????"; }



// ����ṹ������
IAstClass::IAstClass (PublicLevel _level, std::string _module_name, std::string _name):
	m_level (_level), m_module_name (_module_name), m_name (_name) {}

std::optional<size_t> IAstClass::GetVarIndex (std::string _name) { return std::nullopt; }

std::optional<IAstClassItem*> IAstClass::GetMember (std::string _name) {
	for (auto _func : m_funcs) {
		if (_func->m_name == _name)
			return (IAstClassItem*) _func.get ();
	}
	return std::nullopt;
}

std::shared_ptr<AstClassFunc> IAstClass::AddFunc (PublicLevel _pv, bool _is_static, std::string _name) {
	auto _func = std::make_shared<AstClassFunc> (_pv, _is_static, _name);
	m_funcs.push_back (_func);
	return _func;
}



// ��
AstClass::AstClass (PublicLevel _level, std::string _module_name, std::string _name): IAstClass (_level, _module_name, _name) {}

AstClassType AstClass::GetType () { return AstClassType::Class; }

std::optional<llvm::Type*> AstClass::GetLlvmType (std::function<std::optional<llvm::Type*> (std::string, antlr4::Token*)> _cb) {
	if (!m_type) {
		std::vector<llvm::Type*> _v;
		for (auto _var : m_vars) {
			std::string _tmp_type = _var->m_type.substr (_var->m_type.size () - 2) == "[]" ? "cptr" : _var->m_type;
			auto _otype = _cb (_tmp_type, _var->m_t);
			if (!_otype.has_value ())
				return std::nullopt;
			_v.push_back (_otype.value ());
		}
		if (m_vars.size () == 0) {
			auto _otype = _cb ("int8", nullptr);
			if (!_otype.has_value ())
				return std::nullopt;
			_v.push_back (_otype.value ());
		}
		m_type = llvm::StructType::create (_v);
	}
	return (llvm::Type*) m_type;
}

std::optional<IAstClassItem*> AstClass::GetMember (std::string _name) {
	for (auto _var : m_vars) {
		if (_var->m_name == _name)
			return (IAstClassItem*) _var.get ();
	}
	return IAstClass::GetMember (_name);
}

bool AstClass::GetVars (std::function<bool (AstClassVar*)> _cb) {
	for (auto& _var : m_vars) {
		if (!_cb (_var.get ()))
			return false;
	}
	return true;
}



void AstClass::AddParents (std::vector<std::string> &_parents) {
	m_parents.assign (_parents.cbegin (), _parents.cend ());
}

void AstClass::AddVar (std::shared_ptr<AstClassVar> _var) {
	m_vars.push_back (_var);
}

std::optional<size_t> AstClass::GetVarIndex (std::string _name) {
	for (size_t i = 0; i < m_vars.size (); ++i) {
		if (m_vars [i]->m_name == _name)
			return i;
	}
	return std::nullopt;
}

std::optional<AstClassVar*> AstClass::GetVar (size_t _idx) {
	if (_idx >= m_vars.size ())
		return std::nullopt;
	return m_vars [_idx].get ();
}



// �༯��
std::optional<std::shared_ptr<IAstClass>> AstClasses::GetClass (std::string _name, std::string _namesapce, std::vector<std::string>& _uses) {
	if (m_classes.contains (_name))
		return m_classes [_name];
	size_t _p = _namesapce.find ('.');
	std::string _tmp;
	while (_p != std::string::npos) {
		_tmp = std::format ("{}.{}", _namesapce.substr (0, _p), _name);
		if (m_classes.contains (_tmp))
			return m_classes [_tmp];
		_p = _namesapce.find ('.', _p + 1);
	};
	if (_namesapce != "") {
		_tmp = std::format ("{}.{}", _namesapce, _name);
		if (m_classes.contains (_tmp))
			return m_classes [_tmp];
	}
	// TODO ���use�������ռ�
	for (std::string _use : _uses) {
		_tmp = std::format ("{}.{}", _use, _name);
		if (m_classes.contains (_tmp))
			return m_classes [_tmp];
	}
	return std::nullopt;
}

std::shared_ptr<AstClass> AstClasses::CreateNewClass (PublicLevel _level, std::string _module_name, std::string _name) {
	auto _cls = std::make_shared<AstClass> (_level, _module_name, _name);
	m_classes [_name] = _cls;
	return _cls;
}

bool AstClasses::EnumClasses (std::string _module_name, std::function<bool (std::shared_ptr<IAstClass>)> _cb) {
	for (auto &[_key, _val] : m_classes) {
		if (_val->m_module_name != _module_name)
			continue;
		if (!_cb (_val))
			return false;
	}
	return true;
}
