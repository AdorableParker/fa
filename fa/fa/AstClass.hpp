#ifndef __AST_CLASS_HPP__
#define __AST_CLASS_HPP__



#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

//#include "AstValue.hpp"

#include <llvm/IR/Type.h>



enum class PublicLevel { Unknown, Public, Internal, Protected, Private };



// ������� getter setter ����
class AstClassVarFunc {
public:
	PublicLevel						m_pv;					// ��������
	std::string						m_name;					// getter setter ����
	FaParser::ClassFuncBodyContext*	m_func;					// ������

	AstClassVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func)
		: m_pv (_pv), m_name (_name), m_func (_func) {}
};



// �����
class AstClassVar {
public:
	PublicLevel						m_pv;					// ��������
	bool							m_is_static;			// �Ƿ�̬
	std::string						m_type;					// ��������
	std::string						m_name;					// ��������
	FaParser::ExprContext		   *m_init_value = nullptr;	// ��ʼֵ
	std::vector<AstClassVarFunc>	m_var_funcs;			// ���� getter setter ����

	AstClassVar (PublicLevel _pv, bool _is_static, std::string _type, std::string _name)
		: m_pv (_pv), m_is_static (_is_static), m_type (_type), m_name (_name) {}

	void SetInitValue (FaParser::ExprContext* _init_value) { m_init_value = _init_value; }

	std::tuple<bool, std::string> AddVarFunc (PublicLevel _pv, std::string _name, FaParser::ClassFuncBodyContext* _func) {
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
};



// �෽��
class AstClassFunc {
public:
	PublicLevel						m_pv;					// ��������
	bool							m_is_static;			// �Ƿ�̬
	std::string						m_name;					// ��������
	std::string						m_ret_type;				// ��������
	std::vector<std::string>		m_arg_types;			// ���������б�
	std::vector<std::string>		m_arg_names;			// ���������б�
	FaParser::ClassFuncBodyContext *m_func = nullptr;		// ������

	AstClassFunc (PublicLevel _pv, bool _is_static, std::string _name)
		: m_pv (_pv), m_is_static (_is_static), m_name (_name) {}

	void SetReturnType (std::string _ret_type) { m_ret_type = _ret_type; }

	void SetArgumentTypeName (std::string _type, std::string _name) {
		m_arg_types.push_back (_type);
		m_arg_names.push_back (_name);
	}

	void SetFuncBody (FaParser::ClassFuncBodyContext *_func) { m_func = _func; }
};



class AstClass {
public:
	PublicLevel									m_level;
	std::string									m_name;
	std::vector<std::string>					m_parents;
	std::vector<std::shared_ptr<AstClassVar>>	m_vars;
	std::vector<std::shared_ptr<AstClassFunc>>	m_funcs;
	llvm::Type									*m_type = nullptr;

	AstClass (PublicLevel _level, std::string _name): m_level (_level), m_name (_name) {}
	void AddParents (std::vector<std::string> &_parents) {
		m_parents.assign (_parents.cbegin (), _parents.cend ());
	}

	std::shared_ptr<AstClassVar> AddVar (PublicLevel _pv, bool _is_static, std::string _type, std::string _name) {
		auto _var = std::make_shared<AstClassVar> (_pv, _is_static, _type, _name);
		m_vars.push_back (_var);
		return _var;
	}

	std::shared_ptr<AstClassFunc> AddFunc (PublicLevel _pv, bool _is_static, std::string _name) {
		auto _func = std::make_shared<AstClassFunc> (_pv, _is_static, _name);
		m_funcs.push_back (_func);
		return _func;
	}

	std::optional<std::shared_ptr<AstClassVar>> GetVar (std::string _name) {
		for (auto _var : m_vars) {
			if (_var->m_name == _name)
				return _var;
		}
		return std::nullopt;
	}
};



class AstClasses {
public:
	std::optional<std::shared_ptr<AstClass>> GetClass (std::string _name) {
		if (m_classes.contains (_name)) {
			return m_classes [_name];
		} else {
			return std::nullopt;
		}
	}
	std::shared_ptr<AstClass> CreateNewClass (PublicLevel _level, std::string _name) {
		auto _cls = std::make_shared<AstClass> (_level, _name);
		m_classes [_name] = _cls;
		return _cls;
	}

private:
	std::map<std::string, std::shared_ptr<AstClass>> m_classes;
};



#endif //__AST_CLASS_HPP__
